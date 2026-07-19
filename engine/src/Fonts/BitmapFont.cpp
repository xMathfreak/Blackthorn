#include "Fonts/BitmapFont.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <istream>
#include <sstream>

#include <SDL3_image/SDL_image.h>

#include "Debug/Logger.h"
#include "Fonts/FontConfig.h"

namespace {

inline void toLower(std::string& s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

bool parseKeyValue(const std::string& line, const std::string& key, std::string& outValue) {
	std::istringstream iss(line);
	std::string token;

	while (iss >> token) {
		auto eq = token.find('=');

		if (eq != std::string::npos) {
			std::string k = token.substr(0, eq);
			toLower(k);

			std::string v = token.substr(eq + 1);

			if (k == key) {
				if (!v.empty()) {
					v.erase(v.find_last_not_of(" \t") + 1);
					outValue = v;
					return true;
				}

				return false;
			}
		}

		if (token == key) {
			std::string eqToken;
			if (!(iss >> eqToken) || eqToken != "=")
				continue;

			if (iss >> outValue)
				return false;
		}
	}

	return false;
}

int parseIntValue(const std::string& line, const std::string& key) {
	std::string value;
	if (!parseKeyValue(line, key, value))
		return 0;

	try {
		return std::stoi(value);
	} catch (std::invalid_argument&) {
		return 0;
	}
}

float parseFloatValue(const std::string& line, const std::string& key) {
	std::string value;
	if (!parseKeyValue(line, key, value))
		return 0.0f;

	try {
		return std::stof(value);
	} catch (std::invalid_argument&) {
		return 0.0f;
	}
}

}

namespace Blackthorn::Fonts {

std::shared_ptr<Graphics::Shader> BitmapFont::shader = nullptr;

BitmapFont::BitmapFont()
	: cache(FontConfig::getCurrent().maxCachedText)
{
	const FontConfig& cfg = FontConfig::getCurrent();

	MAX_TEXT_GLYPHS = cfg.maxTextGlyphs;
	MAX_VERTICES = cfg.maxTextGlyphs * 4;

	if (shader == nullptr)
		initializeShader();

	initBuffers();
}

BitmapFont::BitmapFont(BitmapFont&& other) noexcept
	: vao(std::move(other.vao))
	, vbo(std::move(other.vbo))
	, texture(std::move(other.texture))
	, glyphs(std::move(other.glyphs))
	, lineHeight(other.lineHeight)
	, spaceWidth(other.spaceWidth)
	, tabWidth(other.tabWidth)
	, cache(std::move(other.cache))
{}

BitmapFont& BitmapFont::operator=(BitmapFont&& other) noexcept {
	if (this != &other) {
		vao = std::move(other.vao);
		vbo = std::move(other.vbo);
		texture = std::move(other.texture);
		glyphs = std::move(other.glyphs);
		lineHeight = other.lineHeight;
		spaceWidth = other.spaceWidth;
		tabWidth = other.tabWidth;
		cache = std::move(other.cache);
	}

	return *this;
}

void BitmapFont::initBuffers() {
	vao = std::make_unique<Graphics::VAO>(true);
	vbo = std::make_unique<Graphics::VBO>(true);

	vao->bind();
	vbo->bind();

	vbo->setData(nullptr, MAX_VERTICES * sizeof(Vertex), GL_DYNAMIC_DRAW);

	vao->enableAttrib(0, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
	vao->enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));

	Graphics::VBO::unbind();
	Graphics::VAO::unbind();
}

bool BitmapFont::loadFromFile(const std::filesystem::path& texturePath, const std::filesystem::path& metricsPath) {
	const auto texPathString = texturePath.string();
	const auto metPathString = metricsPath.string();

	texture = std::make_unique<Graphics::Texture>(texturePath);

	if (!texture->isValid()) {
		BT_ERROR("BitmapFont: Failed to load font texture: {}", texPathString);
		return false;
	}

	std::ifstream file(metricsPath);

	if (!file.is_open()) {
		BT_ERROR("BitmapFont: Failed to load font metrics: {}", metPathString);
		return false;
	}

	return parseMetricsText(file, metPathString);
}

bool BitmapFont::loadFromMemory(const U8* textureData, size_t textureSize, const U8* metricsData, size_t metricsSize) {
	if (!textureData || textureSize == 0 || !metricsData || metricsSize == 0) {
		BT_ERROR("BitmapFont: loadFromMemory called with empty data");
		return false;
	}

	SDL_IOStream* texStream = SDL_IOFromConstMem(textureData, static_cast<int>(textureSize));
	if (!texStream) {
		BT_ERROR("BitmapFont: SDL_IOFromConstMem failed for texture: {}", SDL_GetError());
		return false;
	}

	SDL_Surface* surface = IMG_Load_IO(texStream, true);
	if (!surface) {
		BT_ERROR("BitmapFont: Failed to load font texture from memory: {}", SDL_GetError());
		return false;
	}

	texture = std::make_unique<Graphics::Texture>();
	texture->loadFromSurface(surface);
	SDL_DestroySurface(surface);

	if (!texture->isValid()) {
		BT_ERROR("BitmapFont: Failed to create texture from in-memory font data");
		return false;
	}

	std::string metricsText(reinterpret_cast<const char*>(metricsData), metricsSize);
	std::istringstream metricsStream(metricsText);

	return parseMetricsText(metricsStream, "<memory>");
}

bool BitmapFont::parseMetricsText(std::istream& stream, const std::string& sourceLabel) {
	std::string line;
	int lineNum = 0;

	glyphs.clear();
	baseline = 0.0f;
	lineHeight = 0.0f;

	while (std::getline(stream, line)) {
		lineNum++;

		size_t commentPos = line.find('#');
		if (commentPos != std::string::npos)
			line.resize(commentPos);

		line.erase(0, line.find_first_not_of(" \t"));
		line.erase(line.find_last_not_of(" \t") + 1);

		if (line.empty())
			continue;

		std::istringstream iss(line);
		std::string command;
		iss >> command;
		toLower(command);

		if (command == "common" || command == "global") {
			lineHeight = parseFloatValue(line, "lineheight");
			baseline = parseFloatValue(line, "baseline");

			if (baseline == 0.0f)
				baseline = parseFloatValue(line, "base");

		} else if (command == "char") {
			U32 id = parseIntValue(line, "id");

			if (id == 0) {
				BT_WARN("BitmapFont: Skipping glyph with missing/invalid id in {}", sourceLabel);
				continue;
			}

			Glyph glyph{};
			glyph.rect.x = parseFloatValue(line, "x");
			glyph.rect.y = parseFloatValue(line, "y");

			glyph.rect.w = parseFloatValue(line, "width");
			if (glyph.rect.w == 0.0f)
				glyph.rect.w = parseFloatValue(line, "w");

			glyph.rect.h = parseFloatValue(line, "height");
			if (glyph.rect.h == 0.0f)
				glyph.rect.h = parseFloatValue(line, "h");

			glyph.xOffset = parseIntValue(line, "xoffset");
			glyph.yOffset = parseIntValue(line, "yoffset");
			glyph.xAdvance = parseIntValue(line, "xadvance");

			glyphs[id] = glyph;
		} else {
			BT_WARN(
				"BitmapFont: Unknown command '{}' on line {} in {}",
				command, lineNum, sourceLabel
			);
		}
	}

	if (baseline == 0.0f && lineHeight > 0.0f) {
		for (const auto& [id, glyph] : glyphs)
			baseline = std::max(baseline, static_cast<float>(-glyph.yOffset));

		if (baseline == 0.0f)
			baseline = lineHeight * 0.25f;
	}

	if (glyphs.count(' ')) {
		spaceWidth = glyphs[' '].xAdvance;
	} else {
		spaceWidth = lineHeight * 0.25f;
	}

	tabWidth = spaceWidth * 4.0f;

	BT_DEBUG(
		"BitmapFont loaded\n"
		"    metrics source: {}\n"
		"    glyphs: {}\n"
		"    lineHeight: {:.1f}\n"
		"    baseline: {:.1f}\n"
		"    spaceWidth: {:.1f}",
		sourceLabel,
		glyphs.size(),
		lineHeight,
		baseline,
		spaceWidth
	);

	return true;
}

bool BitmapFont::loadFromBMFont(const std::filesystem::path& bmfPath) {
	const auto pathStr = bmfPath.string();
	std::ifstream file(bmfPath, std::ios::binary);

	if (!file) {
		BT_ERROR("BitmapFont: Failed to open BMF file: {}", pathStr);
		return false;
	}

	return parseBMFontStream(file, pathStr);
}

bool BitmapFont::loadFromBMFontMemory(const U8* data, size_t size) {
	if (!data || size == 0) {
		BT_ERROR("BitmapFont: loadFromBMFontMemory called with empty data");
		return false;
	}

	std::string buffer(reinterpret_cast<const char*>(data), size);
	std::istringstream stream(buffer, std::ios::binary);

	return parseBMFontStream(stream, "<memory>");
}

bool BitmapFont::parseBMFontStream(std::istream& stream, const std::string& sourceLabel) {
	char sign[4];
	stream.read(sign, 4);
	if (sign[0] != 'B' || sign[1] != 'M' || sign[2] != 'F' || sign[3] != '\0') {
		BT_ERROR("BitmapFont: Invalid BMF file format: {}", sourceLabel);
		return false;
	}

	U16 version;
	stream.read(reinterpret_cast<char*>(&version), sizeof(version));

	if (version != 1) {
		BT_ERROR("BitmapFont: Unsupported BMF version {} in file: {}", version, sourceLabel);
		return false;
	}

	stream.read(reinterpret_cast<char*>(&lineHeight), sizeof(float));
	stream.read(reinterpret_cast<char*>(&baseline), sizeof(float));
	stream.read(reinterpret_cast<char*>(&spaceWidth), sizeof(float));

	U32 imageSize;
	stream.read(reinterpret_cast<char*>(&imageSize), sizeof(imageSize));

	std::vector<U8> imageData(imageSize);
	stream.read(reinterpret_cast<char*>(imageData.data()), imageSize);

	SDL_IOStream* rw = SDL_IOFromConstMem(imageData.data(), imageSize);
	if (!rw) {
		BT_ERROR("BitmapFont: Failed to create SDL_IOStream from image data");
		return false;
	}

	SDL_Surface* surface = IMG_Load_IO(rw, true);
	if (!surface) {
		BT_ERROR("BitmapFont: Failed to load image from BMF: {}", SDL_GetError());
		return false;
	}

	texture = std::make_unique<Graphics::Texture>();
	texture->loadFromSurface(surface);
	SDL_DestroySurface(surface);

	if (!texture->isValid()) {
		BT_ERROR("BitmapFont: Failed to create texture from BMF image data");
		return false;
	}

	U32 glyphCount;
	stream.read(reinterpret_cast<char*>(&glyphCount), sizeof(glyphCount));

	glyphs.clear();

	for (U32 i = 0; i < glyphCount; ++i) {
		U32 codePoint;

		Glyph glyph;

		stream.read(reinterpret_cast<char*>(&codePoint), sizeof(codePoint));
		stream.read(reinterpret_cast<char*>(&glyph.rect.x), sizeof(glyph.rect.x));
		stream.read(reinterpret_cast<char*>(&glyph.rect.y), sizeof(glyph.rect.y));
		stream.read(reinterpret_cast<char*>(&glyph.rect.w), sizeof(glyph.rect.w));
		stream.read(reinterpret_cast<char*>(&glyph.rect.h), sizeof(glyph.rect.h));
		stream.read(reinterpret_cast<char*>(&glyph.xOffset), sizeof(glyph.xOffset));
		stream.read(reinterpret_cast<char*>(&glyph.yOffset), sizeof(glyph.yOffset));
		stream.read(reinterpret_cast<char*>(&glyph.xAdvance), sizeof(glyph.xAdvance));

		glyphs[codePoint] = glyph;
	}

	tabWidth = spaceWidth * 4.0f;

	BT_DEBUG(
		"BitmapFont loaded\n"
		"    source: {}\n"
		"    glyphs: {}\n"
		"    lineHeight: {:.1f}\n"
		"    baseline: {:.1f}\n"
		"    spaceWidth: {:.1f}",
		sourceLabel,
		glyphs.size(),
		lineHeight,
		baseline,
		spaceWidth
	);

	return true;
}

Text::Metrics BitmapFont::measure(std::string_view text, float scale, float maxWidth) {
	const Layout layout = buildLayout(text, scale, maxWidth);
	return {
		layout.totalWidth,
		layout.totalHeight,
		layout.lines.size()
	};
}


void BitmapFont::draw(std::string_view text, const glm::vec2& position, float scale, float z, float maxWidth, const Math::Color& color, Text::Alignment alignment) {
	if (!isLoaded() || text.empty())
		return;

	const Layout layout = buildLayout(text, scale, maxWidth);
	vertexBuffer.clear();
	generateVertices(layout, scale, alignment, vertexBuffer);

	if (vertexBuffer.empty())
		return;

	shader->bind();
	shader->setVec3("u_Offset", position.x, position.y, z);
	shader->setVec4("u_Color", color.r, color.g, color.b, color.a);

	vao->bind();
	vbo->updateData(vertexBuffer);
	texture->bind();

	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexBuffer.size()));
}

void BitmapFont::drawCached(std::string_view text, const glm::vec2& position, float scale, float z, float maxWidth, const Math::Color& color, Text::Alignment alignment) {
	if (!isLoaded() || text.empty())
		return;

	TextCacheKey key{ std::string(text), scale, maxWidth, alignment };
	CachedText* cached = cache.get(key);

	if (!cached) {
		CachedText cacheEntry;
		const Layout layout = buildLayout(key.text, scale, maxWidth);

		vertexBuffer.clear();
		generateVertices(layout, scale, alignment, vertexBuffer);

		cacheEntry.vao.create();
		cacheEntry.vbo.create();

		cacheEntry.vao.bind();
		cacheEntry.vbo.setData(vertexBuffer.data(), vertexBuffer.size() * sizeof(Vertex), GL_STATIC_DRAW);
		cacheEntry.vao.enableAttrib(0, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
		cacheEntry.vao.enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));

		cacheEntry.vertexCount = vertexBuffer.size();

		cache.put(key, std::move(cacheEntry));
		cached = cache.get(key);
	}

	shader->bind();
	shader->setVec3("u_Offset", position.x, position.y, z);
	shader->setVec4("u_Color", color.r, color.g, color.b, color.a);

	texture->bind();
	cached->vao.bind();

	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(cached->vertexCount));
	Graphics::VAO::unbind();
	Graphics::Shader::unbind();
}

void BitmapFont::initializeShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/font_bitmap.vert", "assets/shaders/font_bitmap.frag");

		BT_DEBUG("BitmapFont: Shader initialized");
	}
}

void BitmapFont::cleanupShader() {
	shader.reset();
}

BitmapFont::Layout BitmapFont::buildLayout(std::string_view text, float scale, float maxWidth) const {
	Layout layout;
	auto& lines = layout.lines;
	auto& widths = layout.lineWidths;

	if (maxWidth <= 0.0f) {
		size_t start = 0;
		for (size_t i = 0; i < text.length(); ++i) {
			if (text[i] == '\n') {
				lines.push_back(text.substr(start, i - start));
				start = i + 1;
			}
		}

		if (start < text.length())
			lines.push_back(text.substr(start));
	} else {
		size_t lineStart = 0;
		size_t lastSpace = 0;
		float  currentWidth = 0.0f;

		for (size_t i = 0; i < text.length(); ++i) {
			char c = text[i];

			if (c == '\n') {
				lines.push_back(text.substr(lineStart, i - lineStart));
				lineStart = lastSpace = i + 1;
				currentWidth = 0.0f;
				continue;
			}

			float charWidth = 0.0f;
			if (c == ' ') {
				charWidth = spaceWidth * scale;
				lastSpace = i;
			} else if (c == '\t') {
				charWidth = tabWidth * scale;
			} else {
				auto it = glyphs.find(static_cast<U32>(c));
				if (it != glyphs.end())
					charWidth = it->second.xAdvance * scale;
			}

			currentWidth += charWidth;

			if (currentWidth > maxWidth) {
				if (lastSpace > lineStart) {
					lines.push_back(text.substr(lineStart, lastSpace - lineStart));
					lineStart = lastSpace + 1;
					i = lastSpace;
				} else {
					lines.push_back(text.substr(lineStart, i - lineStart));
					lineStart = i;
				}
				currentWidth = 0.0f;
			}
		}

		if (lineStart < text.length())
			lines.push_back(text.substr(lineStart));
	}

	widths.reserve(lines.size());
	for (const auto& line : lines) {
		float w = 0.0f;
		for (char c : line) {
			if (c == ' ') {
				w += spaceWidth * scale;
			} else if (c == '\t') {
				w += tabWidth   * scale;
			} else {
				auto it = glyphs.find(static_cast<U32>(c));
				if (it != glyphs.end())
					w += it->second.xAdvance * scale;
			}
		}

		widths.push_back(w);
		layout.totalWidth = std::max(layout.totalWidth, w);
	}

	layout.totalHeight = lineHeight * scale * static_cast<float>(lines.size());
	return layout;
}

void BitmapFont::generateVertices(const Layout& layout, float scale, Text::Alignment alignment, std::vector<Vertex>& outVertices) const {
	outVertices.clear();
	outVertices.reserve(layout.totalWidth > 0 ? layout.lines.size() * 32 : 8);

	const float texWidth  = static_cast<float>(texture->getWidth());
	const float texHeight = static_cast<float>(texture->getHeight());

	auto snap = [](float n) { return std::floorf(n + 0.5f); };

	float currentY = 0.0f;
	for (size_t li = 0; li < layout.lines.size(); ++li) {
		const auto& line = layout.lines[li];
		float currentX   = 0.0f;

		switch (alignment) {
			case Text::Alignment::Center:
				currentX -= layout.lineWidths[li] * 0.5f;
				break;
			case Text::Alignment::Right:
				currentX -= layout.lineWidths[li];
				break;
			default:
				break;
		}

		for (char c : line) {
			if (c == ' ') {
				currentX += spaceWidth * scale;
				continue;
			} else if (c == '\t') {
				currentX += tabWidth * scale;
				continue;
			}

			auto it = glyphs.find(static_cast<U32>(c));
			if (it == glyphs.end())
				continue;

			const Glyph& glyph = it->second;
			float glyphX = snap(currentX - glyph.xOffset * scale) - 1;
			float glyphY = snap(currentY - (glyph.yOffset - 2) * scale) - 1;
			float glyphW = glyph.rect.w * scale;
			float glyphH = glyph.rect.h * scale;

			float u0 = glyph.rect.x / texWidth;
			float v0 = glyph.rect.y / texHeight;
			float u1 = (glyph.rect.x + glyph.rect.w) / texWidth;
			float v1 = (glyph.rect.y + glyph.rect.h) / texHeight;

			outVertices.push_back({{glyphX,         glyphY        }, {u0, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY        }, {u1, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY + glyphH}, {u1, v1}});
			outVertices.push_back({{glyphX,          glyphY        }, {u0, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY + glyphH}, {u1, v1}});
			outVertices.push_back({{glyphX,          glyphY + glyphH}, {u0, v1}});

			currentX += glyph.xAdvance * scale;
		}

		currentY += lineHeight * scale;
	}
}

} // namespace Blackthorn
