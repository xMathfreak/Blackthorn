#include "Fonts/BitmapFont.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Blackthorn::Fonts {

std::shared_ptr<Graphics::Shader> BitmapFont::shader = nullptr;

BitmapFont::BitmapFont() {
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

bool BitmapFont::loadFromFile(const std::string& texturePath, const std::string& metricsPath) {
	texture = std::make_unique<Graphics::Texture>(texturePath);

	if (!texture->isValid()) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font texture: %s", texturePath.c_str());
		#endif

		return false;
	}

	std::ifstream file(metricsPath);

	if (!file.is_open()) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font metrics: %s", metricsPath.c_str());
		#endif

		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#')
			continue;

		std::istringstream iss(line);
		std::string command;

		iss >> command;

		if (command == "common" || command == "global") {
			std::string key;
			while (iss >> key) {
				size_t eq = key.find('=');
				if (eq != std::string::npos) {
					std::string name = key.substr(0, eq);
					int value = std::stoi(key.substr(eq + 1));

					if (name == "lineHeight") {
						lineHeight = static_cast<float>(value);
					} else if (name == "base" || name == "baseline") {
						baseline = static_cast<float>(value);
					}
				}
			}
		} else if (command == "char") {
			Uint32 id = 0;
			Glyph glyph{};

			std::string key;
			while (iss >> key) {
				size_t eq = key.find('=');
				if (eq != std::string::npos) {
					std::string name = key.substr(0, eq);
					std::string token = key.substr(eq + 1);
					
					if (token.empty())
						continue;

					int value = 0;
					try {
						value = std::stoi(token);
					} catch(const std::invalid_argument&) {
						#ifdef BLACKTHORN_DEBUG
							SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Invalid character %s in font metrics %s", token.c_str(), metricsPath.c_str());
						#endif

						continue;
					}

					if (name == "id") {
						id = value;
					} else if (name == "x") {
						glyph.rect.x = value;
					} else if (name == "y") {
						glyph.rect.y = value;
					} else if (name == "width" || name == "w") {
						glyph.rect.w = value;
					} else if (name == "height" || name == "h") {
						glyph.rect.h = value;
					} else if (name == "xoffset") {
						glyph.xOffset = value;
					} else if (name == "yoffset") {
						glyph.yOffset = value;
					} else if (name == "xadvance") {
						glyph.xAdvance = value;
					}
				}
			}

			if (id == 0) {
				#ifdef BLACKTHORN_DEBUG
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Skipping glyph with missing or invalid id in %s", metricsPath.c_str());
				#endif

				continue;
			}

			glyphs[id] = glyph;
		}
	}

	if (baseline == 0.0f) {
		for (const auto& [id, glyph] : glyphs) {
			baseline = std::max(baseline, static_cast<float>(-glyph.yOffset));
		}
	}

	if (glyphs.count(' ')) {
		spaceWidth = glyphs[' '].xAdvance;
	} else {
		spaceWidth = lineHeight * 0.25f;
	}

	tabWidth = spaceWidth * 4.0f;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("BitmapFont loaded %llu glyphs from %s, lineHeight=%.1f", glyphs.size(), metricsPath.c_str(), lineHeight);
	#endif

	return true;
}

bool BitmapFont::loadFromBMFont(const std::string& bmfPath) {
	return false;
}

float BitmapFont::computeLineWidth(std::string_view line, float scale) const {
	float width = 0.0f;

	for (char c : line) {
		if (c == ' ') {
			width += spaceWidth * scale;
		} else if (c == '\t') {
			width += tabWidth * scale;
		} else {
			auto it = glyphs.find(static_cast<Uint32>(c));
			if (it != glyphs.end())
				width += it->second.xAdvance * scale;
		}
	}

	return width;
}

void BitmapFont::wrapText(std::string_view text, float scale, float maxWidth, std::vector<std::string_view>& outLines) const {
	outLines.clear();

	if (maxWidth <= 0.0f) {
		size_t start = 0;
		for (size_t i = 0; i < text.length(); ++i) {
			if (text[i] == '\n') {
				outLines.push_back(text.substr(start, i - start));
				start = i + 1;
			}
		}

		if (start < text.length())
			outLines.push_back(text.substr(start));

		return;
	}

	size_t lineStart = 0;
	size_t lastSpace = 0;
	float currentWidth = 0.0f;

	for (size_t i = 0; i < text.length(); ++i) {
		char c = text[i];

		if (c == '\n') {
			outLines.push_back(text.substr(lineStart, i - lineStart));
			lineStart = i + 1;
			lastSpace = i + 1;
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
			auto it = glyphs.find(static_cast<Uint32>(c));
			if (it != glyphs.end())
				charWidth = it->second.xAdvance * scale;
		}

		currentWidth += charWidth;

		if (currentWidth > maxWidth) {
			if (lastSpace > lineStart) {
				outLines.push_back(text.substr(lineStart, lastSpace - lineStart));
				lineStart = lastSpace + 1;
				i = lastSpace;
			} else {
				outLines.push_back(text.substr(lineStart, i - lineStart));
				lineStart = i;
			}

			currentWidth = 0.0f;
		}
	}

	if (lineStart < text.length())
		outLines.push_back(text.substr(lineStart));
}

TextMetrics BitmapFont::computeMetrics(std::string_view text, float scale, float maxWidth) const {
	lineBuffer.clear();
	wrapText(text, scale, maxWidth, lineBuffer);

	float maxLineWidth = 0.0f;
	for (const auto& line : lineBuffer) {
		float lineWidth = computeLineWidth(line, scale);
		maxLineWidth = std::max(maxLineWidth, lineWidth);
	}

	return {
		maxLineWidth,
		lineHeight * scale * lineBuffer.size(),
		lineBuffer.size()
	};
}

TextMetrics BitmapFont::measure(std::string_view text, float scale, float maxWidth) {
	return computeMetrics(text, scale, maxWidth);
}

void BitmapFont::generateVertices(std::string_view text, float scale, float maxWidth, TextAlign alignment, std::vector<Vertex>& outVertices) const {
	lineBuffer.clear();
	wrapText(text, scale, maxWidth, lineBuffer);

	outVertices.clear();
	outVertices.reserve(lineBuffer.size() * text.length() * 6);

	float currentY = 0.0f;
	float texWidth = static_cast<float>(texture->getWidth());
	float texHeight = static_cast<float>(texture->getHeight());

	for (const auto& line : lineBuffer) {
		float lineWidth = computeLineWidth(line, scale);
		float currentX = 0.0f;

		switch (alignment) {
			case TextAlign::Center:
				currentX -= lineWidth * 0.5f;
				break;
			case TextAlign::Right:
				currentX -= lineWidth;
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

			auto it = glyphs.find(static_cast<Uint32>(c));
			if (it == glyphs.end())
				continue;

			const Glyph& glyph = it->second;

			float glyphX = currentX + glyph.xOffset * scale;
			float glyphY = currentY + (baseline + glyph.yOffset) * scale;
			float glyphW = glyph.rect.w * scale;
			float glyphH = glyph.rect.h * scale;

			float u0 = glyph.rect.x / texWidth;
			float u1 = (glyph.rect.x + glyph.rect.w) / texWidth;
			float v0 = (glyph.rect.y + glyph.rect.h) / texHeight;
			float v1 = glyph.rect.y / texHeight;

			outVertices.push_back({{glyphX, glyphY}, {u0, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY}, {u1, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY + glyphH}, {u1, v1}});
			
			outVertices.push_back({{glyphX, glyphY}, {u0, v0}});
			outVertices.push_back({{glyphX + glyphW, glyphY + glyphH}, {u1, v1}});
			outVertices.push_back({{glyphX, glyphY + glyphH}, {u0, v1}});

			currentX += glyph.xAdvance * scale;
		}

		currentY += lineHeight * scale;
	}
}

void BitmapFont::draw(std::string_view text, const glm::vec2& position, float scale, float maxWidth, const SDL_FColor& color, TextAlign alignment) {
	if (!isLoaded() || text.empty())
		return;

	vertexBuffer.clear();
	vertexBuffer.reserve(text.length() * 6);
	generateVertices(text, scale, maxWidth, alignment, vertexBuffer);

	if (vertexBuffer.empty())
		return;

	shader->bind();
	shader->setVec2("u_Offset", position.x, position.y);
	shader->setVec4("u_Color", color.r, color.g, color.b, color.a);

	vao->bind();
	vbo->updateData(vertexBuffer);
	texture->bind();

	glDrawArrays(GL_TRIANGLES, 0, vertexBuffer.size());
}

void BitmapFont::drawCached(std::string_view text, const glm::vec2& position, float scale, float maxWidth, const SDL_FColor& color, TextAlign alignment) {
	if (!isLoaded() || text.empty())
		return;

	TextCacheKey key {
		std::string(text), scale, maxWidth, alignment
	};

	CachedText* cached = cache.get(key);

	if (!cached) {
		CachedText cacheEntry;

		vertexBuffer.clear();
		vertexBuffer.reserve(text.length() * 6);
		generateVertices(key.text, scale, maxWidth, alignment, vertexBuffer);
		
		cacheEntry.vao.create();
		cacheEntry.vbo.create();

		cacheEntry.vao.bind();
		cacheEntry.vbo.setData(vertexBuffer.data(), vertexBuffer.size() * sizeof(Vertex), GL_STATIC_DRAW);

		cacheEntry.vao.enableAttrib(0, 3, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
		cacheEntry.vao.enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));
		cacheEntry.vertexCount = vertexBuffer.size();

		cache.put(key, std::move(cacheEntry));
		cached = cache.get(key);
	}

	shader->bind();
	shader->setVec2("u_Offset", position.x, position.y);
	shader->setVec4("u_Color", color.r, color.g, color.b, color.a);

	texture->bind();
	cached->vao.bind();
	
	glDrawArrays(GL_TRIANGLES, 0, cached->vertexCount);
	Graphics::VAO::unbind();
	Graphics::Shader::unbind();
}

void BitmapFont::initializeShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/font_bitmap.vert", "assets/shaders/font_bitmap.frag");

		#ifdef BLACKTHORN_DEBUG
			SDL_Log("BitmapFont Shader initialized");
		#endif
	}
}

void BitmapFont::cleanupShader() {
	shader.reset();
}

} // namespace Blackthorn
