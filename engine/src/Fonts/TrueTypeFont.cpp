#include "Fonts/TrueTypeFont.h"
#include "glm/gtc/type_ptr.hpp"

namespace Blackthorn::Fonts {

std::shared_ptr<Graphics::Shader> TrueTypeFont::shader = nullptr;
Uint32 TrueTypeFont::shaderRefCount = 0;

TrueTypeFont::TrueTypeFont(Graphics::Renderer* ren)
	: ebo(nullptr)
	, vao(nullptr)
	, vbo(nullptr)
	, renderer(ren)
	, font(nullptr)
	, atlasCursor({0, 0})
	, atlasRowHeight(0)
	, fontSize(0)
	, lineHeight(0)
{
	if (shaderRefCount == 0)
		initShader();

	shaderRefCount++;
	initBuffers();
}

TrueTypeFont::~TrueTypeFont() {
	shaderRefCount--;

	if (shaderRefCount == 0)
		cleanShader();

	if (font)
		TTF_CloseFont(font);
}

void TrueTypeFont::initShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/font_ttf.vert", "assets/shaders/font_ttf.frag");

		#ifdef BLACKTHORN_DEBUG
			SDL_Log("Created TrueTypeFont Shader");
		#endif
	}
}

void TrueTypeFont::cleanShader() {
	if (shader)
		shader.reset();
}

bool TrueTypeFont::loadFromFile(const std::string& filePath, int pointSize) {
	font = TTF_OpenFont(filePath.c_str(), pointSize);
	if (!font) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load True Type font '%s': '%s'", filePath.c_str(), SDL_GetError());
		return false;
	}

	fontSize = pointSize;
	lineHeight = TTF_GetFontLineSkip(font);

	atlas = std::make_unique<Graphics::Texture>();
	atlas->create(ATLAS_SIZE, ATLAS_SIZE, 4, {
		.minFilter = Graphics::TextureFilter::Linear,
		.magFilter = Graphics::TextureFilter::Linear,
		.wrapS = Graphics::TextureWrap::ClampToEdge,
		.wrapT = Graphics::TextureWrap::ClampToEdge,
		.generateMipmaps = true
	});

	atlasCursor = {0, 0};
	atlasRowHeight = 0;
	glyphCache.clear();

	SDL_Log("Loaded TrueType font '%s' at %d pt (line height: %f)", filePath.c_str(), pointSize, lineHeight);
	
	return true;
}


void TrueTypeFont::draw(std::string_view text, const glm::vec2& position, float scale, float maxWidth, const SDL_FColor& color, TextAlign alignment) {
	if (!font || text.empty())
		return;

	std::vector<GlyphBatch> batches;
	buildTextGeometry(std::string(text), position.x, position.y, {color.r, color.g, color.b, color.a}, scale, maxWidth, batches);
	renderBatches(batches, position.x, position.y, {color.r, color.g, color.b, color.a}, scale);
}

void TrueTypeFont::drawCached(std::string_view text, const glm::vec2& position, float scale, float maxWidth, const SDL_FColor& color, TextAlign alignment) {

}

TextMetrics TrueTypeFont::measure(std::string_view text, float scale, float maxWidth) const {
	return {};
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
	vao->enableAttrib(1, 4, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, color));
	vao->enableAttrib(2, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));

	Graphics::VBO::unbind();
	Graphics::VAO::unbind();
}

const TrueTypeFont::Glyph& TrueTypeFont::getGlyph(char32_t codePoint) {
	auto it = glyphCache.find(codePoint);
	if (it != glyphCache.end())
		return it->second;

	SDL_Surface* surface = TTF_RenderGlyph_Blended(font, codePoint, SDL_Color{255, 255, 255, 255});

	if (!surface) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to render glyph U+%04X: %s", codePoint, SDL_GetError());

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	if (atlasCursor.x + surface->w > ATLAS_SIZE) {
		atlasCursor.x = 0;
		atlasCursor.y += atlasRowHeight;
		atlasRowHeight = 0;
	}

	if (atlasCursor.y + surface->h > ATLAS_SIZE) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "True Type Font atlas overflow");
		SDL_DestroySurface(surface);

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	atlas->bind();

	SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	atlas->updateRegion(atlasCursor.x, atlasCursor.y, converted->w, converted->h, converted->pixels);
	SDL_DestroySurface(converted);
	
	float u0 = atlasCursor.x / float(ATLAS_SIZE);
	float v0 = atlasCursor.y / float(ATLAS_SIZE);
	float u1 = (atlasCursor.x + surface->w) / float(ATLAS_SIZE);
	float v1 = (atlasCursor.y + surface->h) / float(ATLAS_SIZE);

	int minX, maxX, minY, maxY, advance;
	TTF_GetGlyphMetrics(font, codePoint, &minX, &maxX, &minY, &maxY, &advance);

	Glyph glyph;
	glyph.size = glm::vec2(surface->w, surface->h);
	glyph.bearing = glm::vec2(minX, maxY);
	glyph.advance = static_cast<float>(advance);
	glyph.uv = {u0, v0, u1, v1};

	atlasCursor.x += surface->w;
	atlasRowHeight = std::max(atlasRowHeight, surface->h);

	SDL_DestroySurface(surface);

	glyphCache[codePoint] = std::move(glyph);
	return glyphCache[codePoint];
}

void TrueTypeFont::buildTextGeometry(const std::string& text, float x, float y, const glm::vec4& color, float scale, float maxWidth, std::vector<GlyphBatch>& batches) {
	batches.clear();
	GlyphBatch batch;

	auto codePoints = utf8To32(text);
	auto lines = layoutText(codePoints, maxWidth);

	float cursorY = 0.0f;

	for (const auto& line : lines) {
		for (const auto& lg : line.glyphs) {	
			const Glyph& glyph = *lg.glyph;

			if (glyph.size.x == 0)
				continue;

			float xPos = lg.pos.x;
			float yPos = cursorY;

			float w = glyph.size.x;
			float h = glyph.size.y;

			const auto& uv = glyph.uv;

			batch.vertices.push_back({{xPos,     yPos},     color, {uv.x, uv.w}});
			batch.vertices.push_back({{xPos + w, yPos},     color, {uv.z, uv.w}});
			batch.vertices.push_back({{xPos + w, yPos + h}, color, {uv.z, uv.y}});
			batch.vertices.push_back({{xPos,     yPos + h}, color, {uv.x, uv.y}});

			batch.indices += 6;

		}

		cursorY += lineHeight;
	}
	
	batches.push_back(std::move(batch));
}

void TrueTypeFont::renderBatches(const std::vector<GlyphBatch>& batches, float x, float y, const glm::vec4& color, float scale) {
	if (batches.empty())
		return;

	shader->bind();

	shader->setMat4("u_Projection", glm::value_ptr(renderer->getViewProjectionMatrix()));
	shader->setVec2("u_Position", x, y);
	shader->setFloat("u_Scale", scale);
	shader->setVec4("u_Color", color.x, color.y, color.z, color.w);
	shader->setInt("u_Texture", 0);

	vao->bind();
	vbo->bind();
	ebo->bind();
	atlas->bind();

	const auto& batch = batches.front();

	vbo->updateData(batch.vertices);

	glDrawElements(GL_TRIANGLES, batch.indices, GL_UNSIGNED_INT, nullptr);
}

std::vector<char32_t> TrueTypeFont::utf8To32(const std::string& utf8) const {
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

	for (size_t i = 0; i < text.size(); ++i) {
		char32_t c = text[i];

		if (c == U'\n') {
			newLine();
			continue;
		}

		const Glyph& g = getGlyph(c);
		float advance = g.advance;

		if (maxWidth > 0.0f && cursorX + advance > maxWidth)
			newLine();

		LayoutGlyph lg;
		lg.glyph = &g;
		lg.pos = {cursorX, 0.0f};

		lines.back().glyphs.push_back(lg);
		lines.back().width += advance;

		cursorX += advance;

	}
	
	return lines;
}


} // namespace Blackthorn::Fonts