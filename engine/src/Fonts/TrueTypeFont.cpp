#include "Fonts/TrueTypeFont.h"

#include <format>

#include "Debug/Logger.h"

namespace Blackthorn::Fonts {

std::shared_ptr<Graphics::Shader> TrueTypeFont::shader = nullptr;

TrueTypeFont::TrueTypeFont() {
	if (!shader)
		initializeShader();

	initBuffers();
}

TrueTypeFont::~TrueTypeFont() {
	if (font)
		TTF_CloseFont(font);
}

void TrueTypeFont::initializeShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/font_ttf.vert", "assets/shaders/font_ttf.frag");

		BT_DEBUG("Created TrueTypeFont Shader");
	}
}

void TrueTypeFont::cleanupShader() {
	if (shader)
		shader->destroy();
}

bool TrueTypeFont::loadFromFile(const std::string& filePath, int pointSize) {
	font = TTF_OpenFont(filePath.c_str(), pointSize);
	if (!font) {
		BT_ERROR(std::format("Failed to load True Type font '{}': '{}'", filePath, SDL_GetError()));
		return false;
	}

	TTF_SetFontSDF(font, true);
	TTF_SetFontKerning(font, true);

	lineHeight = TTF_GetFontLineSkip(font);
	descent = TTF_GetFontDescent(font);

	atlas = std::make_unique<Graphics::Texture>();
	atlas->create(ATLAS_SIZE, ATLAS_SIZE, 1, {
		.minFilter = Graphics::TextureFilter::Linear,
		.magFilter = Graphics::TextureFilter::Linear,
		.wrapS = Graphics::TextureWrap::ClampToEdge,
		.wrapT = Graphics::TextureWrap::ClampToEdge,
		.generateMipmaps = false
	});

	atlasCursor = {0, 0};
	atlasRowHeight = 0;
	glyphCache.clear();

	BT_DEBUG(std::format(
		"Loaded TrueType font '{}' at {} pt (line height: {})",
		filePath, pointSize, lineHeight
	));

	return true;
}


void TrueTypeFont::draw(std::string_view text, const glm::vec2& position, float scale, float z, float maxWidth, const Math::Color& color, Text::Alignment alignment) {
	if (!font || text.empty())
		return;

	float layoutWidth = (maxWidth > 0.0f && scale > 0.0f) ? maxWidth / scale : maxWidth;

	std::vector<Vertex> vertices;
	GLsizei indices = 0;
	generateVertices(text, layoutWidth, alignment, vertices, indices);
	render(vertices, indices, position, scale, z, color);
}

void TrueTypeFont::drawCached(std::string_view text, const glm::vec2& position, float scale, float z, float maxWidth, const Math::Color& color, Text::Alignment alignment) {
	if (!font || text.empty())
		return;

	TextCacheKey key {
		std::string(text), scale, maxWidth, alignment
	};

	CachedText* cached = textCache.get(key);
	if (!cached) {
		CachedText cacheEntry;
		float layoutWidth = (maxWidth > 0.0f && scale > 0.0f) ? maxWidth / scale : maxWidth;
		generateVertices(text, layoutWidth, alignment, cacheEntry.vertices, cacheEntry.indexCount);
		textCache.put(key, std::move(cacheEntry));
		cached = textCache.get(key);
	}

	render(cached->vertices, cached->indexCount, position, scale, z, color);
}

Text::Metrics TrueTypeFont::measure(std::string_view text, float scale, float maxWidth) {
	Text::Metrics metrics{0.0f, 0.0f, 0};

	if (!font || text.empty())
		return metrics;

	float layoutWidth = (maxWidth > 0.0f && scale > 0.0f) ? maxWidth / scale : maxWidth;

	auto codePoints = utf8To32(text);
	auto lines = layoutText(codePoints, layoutWidth);

	metrics.lineCount = lines.size();
	metrics.height = lines.size() * lineHeight * scale;

	float maxLineWidth = 0.0f;
	for (const auto& line : lines)
		maxLineWidth = std::max(maxLineWidth, line.width);

	metrics.width = maxLineWidth * scale;

	return metrics;
}

float TrueTypeFont::getLineHeight() const {
	return lineHeight;
}

void TrueTypeFont::setStyle(TTF_FontStyleFlags style) {
	if (font) {
		TTF_SetFontStyle(font, style);
		glyphCache.clear();
	}
}

void TrueTypeFont::setOutline(int outline) {
	if (font) {
		TTF_SetFontOutline(font, outline);
		glyphCache.clear();
	}
}

void TrueTypeFont::setHinting(TTF_HintingFlags hinting) {
	if (font) {
		TTF_SetFontHinting(font, hinting);
		glyphCache.clear();
	}
}

void TrueTypeFont::setKerning(bool enabled) {
	if (font) {
		TTF_SetFontKerning(font, enabled);
		glyphCache.clear();
	}
}

void TrueTypeFont::initBuffers() {
	vao = std::make_unique<Graphics::VAO>(true);
	vbo = std::make_unique<Graphics::VBO>(true);
	ebo = std::make_unique<Graphics::EBO>(true);

	vao->bind();
	vbo->bind();
	ebo->bind();

	vbo->setData(nullptr, MAX_VERTICES * sizeof(Vertex), GL_DYNAMIC_DRAW);

	std::vector<GLuint> indices;
	indices.reserve(MAX_INDICES);

	Uint32 offset = 0;
	for (Uint32 i = 0; i < MAX_INDICES; i += 6) {
		indices.push_back(offset + 0);
		indices.push_back(offset + 1);
		indices.push_back(offset + 2);

		indices.push_back(offset + 2);
		indices.push_back(offset + 3);
		indices.push_back(offset + 0);

		offset += 4;
	}

	ebo->setData(indices);

	vao->enableAttrib(0, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
	vao->enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));

	Graphics::VBO::unbind();
	Graphics::VAO::unbind();
}

const TrueTypeFont::Glyph& TrueTypeFont::getGlyph(char32_t codePoint) {
	auto it = glyphCache.find(codePoint);
	if (it != glyphCache.end())
		return it->second;

	SDL_Surface* surface = TTF_RenderGlyph_Blended(font, codePoint, SDL_Color{255, 255, 255, 255});

	if (!surface) {
		BT_WARN(std::format("Failed to render glyph U+{:#x}: {}", static_cast<Uint32>(codePoint), SDL_GetError()));

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	if (atlasCursor.x + surface->w > ATLAS_SIZE) {
		atlasCursor.x = 0;
		atlasCursor.y += atlasRowHeight;
		atlasRowHeight = 0;
	}

	if (atlasCursor.y + surface->h > ATLAS_SIZE) {
		BT_ERROR("TrueTypeFont atlas overflow");
		SDL_DestroySurface(surface);

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	const int pixelCount = surface->w * surface->h;
	reuseBuffer.resize(pixelCount);

	const Uint8* src = static_cast<Uint8*>(surface->pixels);
	const int pitch = surface->pitch;

	for (int row = 0; row < surface->h; ++row) {
		const Uint32* rowPixels = reinterpret_cast<const Uint32*>(src + row * pitch);
		for (int col = 0; col < surface->w; ++col)
			reuseBuffer[row * surface->w + col] = (rowPixels[col] >> 24) & 0xFF;
	}

	atlas->updateRegion(atlasCursor.x, atlasCursor.y, surface->w, surface->h, reuseBuffer.data());

	float u0 = atlasCursor.x / float(ATLAS_SIZE);
	float v0 = (atlasCursor.y + surface->h) / float(ATLAS_SIZE);
	float u1 = (atlasCursor.x + surface->w) / float(ATLAS_SIZE);
	float v1 = atlasCursor.y / float(ATLAS_SIZE);

	int minX, maxX, minY, maxY, advance;
	TTF_GetGlyphMetrics(font, codePoint, &minX, &maxX, &minY, &maxY, &advance);

	Glyph glyph;
	glyph.size = glm::vec2(surface->w, surface->h);
	glyph.advance = static_cast<float>(advance);
	glyph.uv = {u0, v0, u1, v1};

	atlasCursor.x += surface->w;
	atlasRowHeight = std::max(atlasRowHeight, surface->h);

	SDL_DestroySurface(surface);

	glyphCache[codePoint] = std::move(glyph);
	return glyphCache[codePoint];
}

void TrueTypeFont::generateVertices(std::string_view text, float maxWidth, Text::Alignment alignment, std::vector<Vertex>& outVertices, GLsizei& outIndexCount) {
	outVertices.clear();
	outIndexCount = 0;

	auto codePoints = utf8To32(text);
	auto lines = layoutText(codePoints, maxWidth);

	float cursorY = 0.0f;

	for (const auto& line : lines) {
		float offsetX = 0.0f;

		switch (alignment) {
			case Text::Alignment::Center:
				offsetX -= line.width * 0.5f;
				break;
			case Text::Alignment::Right:
				offsetX -= line.width;
				break;
			default:
				break;
		}

		for (const auto& lg : line.glyphs) {
			const Glyph& glyph = *lg.glyph;

			float xPos = lg.xPos + offsetX;
			float yPos = cursorY;

			float w = glyph.size.x;
			float h = glyph.size.y;

			if (w == 0 || h == 0)
				continue;

			const auto& uv = glyph.uv;

			outVertices.push_back({{xPos, yPos},{uv.x, uv.w}});
			outVertices.push_back({{xPos + w, yPos}, {uv.z, uv.w}});
			outVertices.push_back({{xPos + w, yPos + h}, {uv.z, uv.y}});
			outVertices.push_back({{xPos, yPos + h}, {uv.x, uv.y}});

			outIndexCount += 6;

		}

		cursorY += lineHeight;
	}
}

void TrueTypeFont::render(const std::vector<Vertex>& vertices, GLsizei indexCount, const glm::vec2& position, float scale, float z, const Math::Color& color) {
	if (vertices.empty())
		return;

	shader->bind();

	shader->setVec3("u_Offset", position.x, position.y, z);
	shader->setFloat("u_Scale", scale);
	shader->setVec4("u_Color", color.x, color.y, color.z, color.w);
	shader->setInt("u_Texture", 0);

	vao->bind();
	vbo->bind();
	ebo->bind();
	atlas->bind();

	vbo->updateData(vertices);

	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

std::vector<char32_t> TrueTypeFont::utf8To32(std::string_view utf8) const {
	std::vector<char32_t> result;
	result.reserve(utf8.size());

	size_t i = 0;
	while (i < utf8.size()) {
		char32_t codePoint = 0;
		unsigned char byte = utf8[i];

		if (byte < 0x80) {
			// 1-byte character
			codePoint = byte;
			i += 1;
		} else if ((byte & 0xE0) == 0xC0) {
			// 2-byte character
			if (i + 1 < utf8.size()) {
				codePoint = ((byte & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
				i += 2;
			} else break;
		} else if ((byte & 0xF0) == 0xE0) {
			// 3-byte character
			if (i + 2 < utf8.size()) {
				codePoint = ((byte & 0x0F) << 12) |
						   ((utf8[i + 1] & 0x3F) << 6) |
						   (utf8[i + 2] & 0x3F);
				i += 3;
			} else break;
		} else if ((byte & 0xF8) == 0xF0) {
			// 4-byte character
			if (i + 3 < utf8.size()) {
				codePoint = ((byte & 0x07) << 18) |
						   ((utf8[i + 1] & 0x3F) << 12) |
						   ((utf8[i + 2] & 0x3F) << 6) |
						   (utf8[i + 3] & 0x3F);
				i += 4;
			} else break;
		} else {
			// Invalid UTF-8
			i += 1;
			continue;
		}

		result.push_back(codePoint);
	}

	return result;
}

std::vector<TrueTypeFont::LayoutLine> TrueTypeFont::layoutText(const std::vector<char32_t>& text, float maxWidth) {
	std::vector<LayoutLine> lines;
	lines.emplace_back();

	float cursorX = 0.0f;

	auto newLine = [&]() {
		lines.emplace_back();
		cursorX = 0;
	};

	const Glyph& spaceGlyph = getGlyph(U' ');
	float spaceAdvance = spaceGlyph.advance;

	size_t i = 0;

	while (i < text.size()) {
		char32_t c = text[i];

		if (c == U'\n') {
			newLine();
			++i;
			continue;
		}

		if (c == U'\t') {
			float tabWidth = TAB_SPACES * spaceAdvance;
			float nextTabStop = std::ceil(cursorX / tabWidth) * tabWidth;

			if (nextTabStop == cursorX)
				nextTabStop += tabWidth;

			if (maxWidth > 0.0f && nextTabStop > maxWidth) {
				newLine();
			} else {
				cursorX = nextTabStop;
			}

			++i;

			continue;
		}

		if (c == U' ') {
			if (maxWidth <= 0.0f || cursorX + spaceAdvance <= maxWidth)
				cursorX += spaceAdvance;

			++i;
			continue;
		}

		float wordWidth = 0.0f;
		struct WordGlyph { const Glyph* glyph; float kern; };
		std::vector<WordGlyph> word;

		char32_t prev = 0;

		for (size_t j = i; j < text.size(); ++j) {
			char32_t wc = text[j];

			if (wc == U' ' || wc == U'\t' || wc == U'\n')
				break;

			const Glyph& g = getGlyph(wc);

			int kern = 0;
			if (prev)
				TTF_GetGlyphKerning(font, prev, wc, &kern);

			word.push_back({&g, static_cast<float>(kern)});
			wordWidth += kern + g.advance;
			prev = wc;
		}

		if (maxWidth > 0.0f && cursorX > 0.0f && cursorX + wordWidth > maxWidth)
			newLine();

		for (const auto& wg : word) {
			cursorX += wg.kern;
			lines.back().glyphs.push_back({wg.glyph, cursorX});
			lines.back().width = cursorX + wg.glyph->advance;
			cursorX += wg.glyph->advance;
		}

		i += word.size();
	}

	return lines;
}

} // namespace Blackthorn::Fonts