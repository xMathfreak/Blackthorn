#include "Fonts/TrueTypeFont.h"

#include <algorithm>

#include "Fonts/FontConfig.h"
#include "Debug/Logger.h"

namespace Blackthorn::Fonts {

std::shared_ptr<Graphics::Shader> TrueTypeFont::shader = nullptr;

TrueTypeFont::TrueTypeFont()
	: textCache(FontConfig::getCurrent().maxCachedText)
	, dynamicTextCache(FontConfig::getCurrent().maxCachedText)
{
	const FontConfig& cfg = FontConfig::getCurrent();

	MAX_TEXT_GLYPHS = cfg.maxTextGlyphs;
	MAX_VERTICES = cfg.maxTextGlyphs * 4;
	MAX_INDICES = cfg.maxTextGlyphs * 6;
	TAB_SPACES = cfg.tabSpaces;

	if (!shader)
		initializeShader();

	initBuffers();
	initDynamicBuffers();
}

TrueTypeFont::~TrueTypeFont() {
	if (font)
		TTF_CloseFont(font);
}

void TrueTypeFont::initializeShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/font_ttf.vert", "assets/shaders/font_ttf.frag");

		BT_DEBUG("TrueTypeFont: Shader initialized");
	}
}

void TrueTypeFont::cleanupShader() {
	if (shader)
		shader->destroy();
}

bool TrueTypeFont::loadFromFile(const std::filesystem::path& filePath, int pointSize) {
	const auto pathStr = filePath.string();
	font = TTF_OpenFont(pathStr.c_str(), static_cast<float>(pointSize));
	if (!font) {
		BT_ERROR("TrueTypeFont: Failed to load True Type font '{}': '{}'", pathStr, SDL_GetError());
		return false;
	}

	// This instance previously owned a loadFromMemory() buffer; a file-backed
	// font doesn't need it.
	fontDataBuffer.clear();
	fontDataBuffer.shrink_to_fit();

	TTF_SetFontSDF(font, true);
	TTF_SetFontKerning(font, true);

	lineHeight = TTF_GetFontLineSkip(font);

	initializeAtlas();

	BT_DEBUG(
		"Loaded TrueType font '{}' at {} pt (line height: {})",
		pathStr, pointSize, lineHeight
	);

	return true;
}

bool TrueTypeFont::loadFromMemory(const U8* data, size_t size, int pointSize) {
	if (!data || size == 0) {
		BT_ERROR("TrueTypeFont: loadFromMemory called with empty data");
		return false;
	}

	if (font) {
		TTF_CloseFont(font);
		font = nullptr;
	}

	fontDataBuffer.assign(data, data + size);

	SDL_IOStream* stream = SDL_IOFromConstMem(fontDataBuffer.data(), fontDataBuffer.size());
	if (!stream) {
		BT_ERROR("TrueTypeFont: SDL_IOFromConstMem failed: '{}'", SDL_GetError());
		fontDataBuffer.clear();
		return false;
	}

	font = TTF_OpenFontIO(stream, true, static_cast<float>(pointSize));
	if (!font) {
		BT_ERROR("TrueTypeFont: Failed to load True Type font from memory: '{}'", SDL_GetError());
		fontDataBuffer.clear();
		return false;
	}

	TTF_SetFontSDF(font, true);
	TTF_SetFontKerning(font, true);

	lineHeight = TTF_GetFontLineSkip(font);

	initializeAtlas();

	BT_DEBUG(
		"Loaded TrueType font from memory ({} bytes) at {} pt (line height: {})",
		size, pointSize, lineHeight
	);

	return true;
}

void TrueTypeFont::initializeAtlas() {
	atlas = std::make_unique<Graphics::Texture>();

	const int atlasSize = FontConfig::getCurrent().atlasSize;

	atlas->create(atlasSize, atlasSize, 1, {
		.minFilter = Graphics::TextureFilter::Linear,
		.magFilter = Graphics::TextureFilter::Linear,
		.wrapS = Graphics::TextureWrap::ClampToEdge,
		.wrapT = Graphics::TextureWrap::ClampToEdge,
		.generateMipmaps = false
	});

	atlasCursor = {0, 0};
	atlasRowHeight = 0;
	glyphCache.clear();
}


void TrueTypeFont::draw(std::string_view text, const glm::vec2& position, const Text::DrawParams& params) {
	if (!font || text.empty())
		return;

	std::string plain;
	std::vector<TextStyle> markupStyles;
	std::string_view renderText = text;

	if (params.useMarkup) {
		auto m = parseMarkup(text);
		plain = std::move(m.plainText);
		markupStyles = std::move(m.charStyles);
		renderText = plain;
	}

	float layoutWidth = (params.maxWidth > 0.0f && params.scale > 0.0f) ? params.maxWidth / params.scale : params.maxWidth;

	std::vector<Vertex> vertices;
	GLsizei indices = 0;
	generateVertices(renderText, layoutWidth, params.alignment, vertices, indices, params.useMarkup ? &markupStyles : nullptr);
	renderStatic(vertices, indices, position, params.scale, params.z, params.color);
}

void TrueTypeFont::drawCached(std::string_view text, const glm::vec2& position, const Text::DrawParams& params) {
	if (!font || text.empty())
		return;

	TextCacheKey key {
		std::string(text), params.scale, params.maxWidth, params.alignment, params.useMarkup
	};

	CachedText* cached = textCache.get(key);
	if (!cached) {
		CachedText cacheEntry;

		std::string plain;
		std::vector<TextStyle> markupStyles;
		std::string_view renderText = text;

		if (params.useMarkup) {
			auto m = parseMarkup(text);
			plain = std::move(m.plainText);
			markupStyles = std::move(m.charStyles);
			renderText = plain;
		}

		float layoutWidth = (params.maxWidth > 0.0f && params.scale > 0.0f) ? params.maxWidth / params.scale : params.maxWidth;

		std::vector<Vertex> vertices;
		GLsizei indexCount = 0;
		generateVertices(renderText, layoutWidth, params.alignment, vertices, indexCount, params.useMarkup ? &markupStyles : nullptr);

		cacheEntry.vao.create();
		cacheEntry.vbo.create();

		cacheEntry.vao.bind();
		cacheEntry.vbo.setData(vertices.data(), vertices.size() * sizeof(Vertex), GL_STATIC_DRAW);
		cacheEntry.vao.enableAttrib(0, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
		cacheEntry.vao.enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoord));
		cacheEntry.vao.enableAttrib(2, 4, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, color));

		ebo->bind();

		cacheEntry.indexCount = indexCount;

		textCache.put(key, std::move(cacheEntry));
		cached = textCache.get(key);
	}

	renderCached(*cached, position, params.scale, params.z, params.color);
}

Text::Metrics TrueTypeFont::measure(std::string_view text, float scale, float maxWidth, bool useMarkup) {
	Text::Metrics metrics{0.0f, 0.0f, 0};

	if (!font || text.empty())
		return metrics;

	std::string plain;
	std::string_view lt = text;

	if (useMarkup) {
		auto m = parseMarkup(text);
		plain = std::move(m.plainText);
		lt = plain;
	}

	float layoutWidth = (maxWidth > 0.0f && scale > 0.0f) ? maxWidth / scale : maxWidth;

	auto codePoints = utf8To32(lt);
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

	U32 offset = 0;
	for (U32 i = 0; i < MAX_INDICES; i += 6) {
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
	vao->enableAttrib(2, 4, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, color));

	Graphics::VBO::unbind();
	Graphics::VAO::unbind();
}

void TrueTypeFont::initDynamicBuffers() {
	dynVAO = std::make_unique<Graphics::VAO>(true);
	dynVBO = std::make_unique<Graphics::VBO>(true);

	dynVAO->bind();
	dynVBO->bind();

	dynVBO->setData(nullptr, MAX_TEXT_GLYPHS * sizeof(Text::GlyphInstance), GL_DYNAMIC_DRAW);
	size_t stride = sizeof(Text::GlyphInstance);

	dynVAO->enableAttrib(3, 2, GL_FLOAT, stride, offsetof(Text::GlyphInstance, position));
	glVertexAttribDivisor(3, 1);
	dynVAO->enableAttrib(4, 2, GL_FLOAT, stride, offsetof(Text::GlyphInstance, size));
	glVertexAttribDivisor(4, 1);
	dynVAO->enableAttrib(5, 4, GL_FLOAT, stride, offsetof(Text::GlyphInstance, uv));
	glVertexAttribDivisor(5, 1);
	dynVAO->enableAttrib(6, 4, GL_FLOAT, stride, offsetof(Text::GlyphInstance, color));
	glVertexAttribDivisor(6, 1);
	dynVAO->enableAttrib(7, 1, GL_FLOAT, stride, offsetof(Text::GlyphInstance, z));
	glVertexAttribDivisor(7, 1);
	dynVAO->enableAttrib(8, 1, GL_FLOAT, stride, offsetof(Text::GlyphInstance, rotation));
	glVertexAttribDivisor(8, 1);
	dynVAO->enableAttrib(9, 2, GL_FLOAT, stride, offsetof(Text::GlyphInstance, shadowOffset));
	glVertexAttribDivisor(9, 1);
	dynVAO->enableAttrib(10, 4, GL_FLOAT, stride, offsetof(Text::GlyphInstance, shadowColor));
	glVertexAttribDivisor(10, 1);
	dynVAO->enableAttrib(11, 1, GL_FLOAT, stride, offsetof(Text::GlyphInstance, shadowBlur));
	glVertexAttribDivisor(11, 1);
	dynVAO->enableAttrib(12, 2, GL_FLOAT, stride, offsetof(Text::GlyphInstance, shake));
	glVertexAttribDivisor(12, 1);

	Graphics::VAO::unbind();
	Graphics::VBO::unbind();
}

const TrueTypeFont::Glyph& TrueTypeFont::getGlyph(char32_t codePoint) {
	auto it = glyphCache.find(codePoint);
	if (it != glyphCache.end())
		return it->second;

	SDL_Surface* surface = TTF_RenderGlyph_Blended(font, codePoint, SDL_Color{255, 255, 255, 255});

	if (!surface) {
		BT_WARN(
			"TrueTypeFont: Failed to render glyph U+{:#x}: {}",
			static_cast<U32>(codePoint), SDL_GetError()
		);

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	const int atlasSize = FontConfig::getCurrent().atlasSize;

	if (atlasCursor.x + surface->w > atlasSize) {
		atlasCursor.x = 0;
		atlasCursor.y += atlasRowHeight;
		atlasRowHeight = 0;
	}

	if (atlasCursor.y + surface->h > atlasSize) {
		BT_ERROR("TrueTypeFont: Atlas overflow");
		SDL_DestroySurface(surface);

		static Glyph defaultGlyph;
		return defaultGlyph;
	}

	const int pixelCount = surface->w * surface->h;
	reuseBuffer.resize(pixelCount);

	const U8* src = static_cast<U8*>(surface->pixels);
	const int pitch = surface->pitch;

	for (int row = 0; row < surface->h; ++row) {
		const U32* rowPixels = reinterpret_cast<const U32*>(src + row * pitch);
		for (int col = 0; col < surface->w; ++col)
			reuseBuffer[row * surface->w + col] = (rowPixels[col] >> 24) & 0xFF;
	}

	atlas->updateRegion(atlasCursor.x, atlasCursor.y, surface->w, surface->h, reuseBuffer.data());

	float u0 = atlasCursor.x / float(atlasSize);
	float v0 = (atlasCursor.y + surface->h) / float(atlasSize);
	float u1 = (atlasCursor.x + surface->w) / float(atlasSize);
	float v1 = atlasCursor.y / float(atlasSize);

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

void TrueTypeFont::generateVertices(std::string_view text, float maxWidth, Text::Alignment alignment, std::vector<Vertex>& outVertices, GLsizei& outIndexCount, const std::vector<TextStyle>* markup) {
	outVertices.clear();
	outIndexCount = 0;

	auto codePoints = utf8To32(text);
	auto lines = layoutText(codePoints, maxWidth);

	float cursorY = 0;

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

			Math::Color glyphColor = Math::Colors::White;
			if (markup && lg.charIndex < markup->size())
				glyphColor = (*markup)[lg.charIndex].color;

			const auto& uv = glyph.uv;

			outVertices.push_back({{xPos, yPos},{uv.x, uv.w}, glyphColor});
			outVertices.push_back({{xPos + w, yPos}, {uv.z, uv.w}, glyphColor});
			outVertices.push_back({{xPos + w, yPos + h}, {uv.z, uv.y}, glyphColor});
			outVertices.push_back({{xPos, yPos + h}, {uv.x, uv.y}, glyphColor});

			outIndexCount += 6;

		}

		cursorY += lineHeight;
	}
}

void TrueTypeFont::renderStatic(const std::vector<Vertex>& vertices, GLsizei indexCount, const glm::vec2& position, float scale, float z, const Math::Color& color) {
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

	vbo->waitFence();
	vbo->updateData(vertices);

	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
	vbo->lockRange();
}

void TrueTypeFont::generateInstances(std::string_view text, float maxWidth, Text::Alignment alignment, std::vector<Text::GlyphInstance>& out, float& outWidth, float& outHeight, const std::vector<TextStyle>* markup) {
	out.clear();
	auto codePoints = utf8To32(text);
	auto lines = layoutText(codePoints, maxWidth);

	float cursorY = 0;
	outWidth = 0.0f;

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
			const Glyph& g = *lg.glyph;

			if (g.size.x == 0 || g.size.y == 0)
				continue;

			Math::Color col = Math::Colors::White;
			glm::vec2 shake(0.0f, 0.0f);

			if (markup && lg.charIndex < markup->size()) {
				const TextStyle& style = (*markup)[lg.charIndex];
				col = style.color;
				shake = glm::vec2(style.shakeStrength, style.shakeSpeed);
			}

			Text::GlyphInstance inst;
			inst.position = glm::vec2(lg.xPos + offsetX, cursorY);
			inst.size = g.size;
			inst.uv = g.uv;
			inst.color = col;
			inst.z = 0.0f;
			inst.scale = 1.0f;
			inst.rotation = 0.0f;
			inst.shadowOffset = glm::vec2(0.0f, 0.0f);
			inst.shadowColor = Math::Color(0.0f, 0.0f, 0.0f, 0.0f);
			inst.shadowBlur = 0.0f;
			inst.shake = shake;

			out.push_back(inst);
		}

		outWidth = std::max(outWidth, line.width);
		cursorY += lineHeight;
	}

	outHeight = cursorY;
}

TrueTypeFont::DynamicText TrueTypeFont::buildDynamic(std::string_view text, float maxWidth, Text::Alignment alignment, bool useMarkup, float scale) {
	DynamicText result;

	if (!font || text.empty())
		return result;

	std::string plain;
	std::vector<TextStyle> markupStyles;
	std::string_view layoutText = text;

	if (useMarkup) {
		auto parsed = parseMarkup(text);
		plain = std::move(parsed.plainText);
		markupStyles = std::move(parsed.charStyles);
		layoutText = plain;
	}

	float layoutWidth = (maxWidth > 0.0f && scale > 0.0f) ? maxWidth / scale : maxWidth;

	generateInstances(layoutText, layoutWidth, alignment, result.instances, result.width, result.height, useMarkup ? &markupStyles : nullptr);

	return result;
}

void TrueTypeFont::renderDynamic(
	const std::vector<Text::GlyphInstance>& instances,
	const glm::vec2& position,
	float scale,
	float z,
	const Math::Color& color)
{
	if (instances.empty())
		return;

	std::vector<Text::GlyphInstance> ordered = instances;
	auto shadowEnd = std::partition(ordered.begin(), ordered.end(), [](const Text::GlyphInstance& inst) {
		return inst.shadowColor.a > 0.01f;
	});

	const GLsizei shadowCount = static_cast<GLsizei>(std::distance(ordered.begin(), shadowEnd));
	const GLsizei totalCount = static_cast<GLsizei>(ordered.size());

	shader->bind();

	shader->setVec3("u_Offset", position.x, position.y, z);
	shader->setFloat("u_Scale", scale);
	shader->setVec4("u_Color", color.r, color.g, color.b, color.a);
	shader->setBool("u_DynamicMode", true);
	shader->setInt("u_Texture", 0);

	atlas->bind();

	dynVAO->bind();
	dynVBO->bind();

	dynVBO->waitFence();
	dynVBO->updateData(ordered);

	if (shadowCount > 0) {
		shader->setBool("u_ShadowPass", true);
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, shadowCount);
	}

	shader->setBool("u_ShadowPass", false);

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, totalCount);
	dynVBO->lockRange();

	Graphics::VAO::unbind();
	Graphics::Shader::unbind();
}

void TrueTypeFont::renderCached(const CachedText& cached, const glm::vec2& position, float scale, float z, const Math::Color& color) {
	if (cached.indexCount == 0)
		return;

	shader->bind();

	shader->setVec3("u_Offset", position.x, position.y, z);
	shader->setFloat("u_Scale", scale);
	shader->setVec4("u_Color", color.x, color.y, color.z, color.w);
	shader->setInt("u_Texture", 0);

	atlas->bind();
	cached.vao.bind();

	glDrawElements(GL_TRIANGLES, cached.indexCount, GL_UNSIGNED_INT, nullptr);

	Graphics::VAO::unbind();
	Graphics::Shader::unbind();
}

void TrueTypeFont::drawDynamic(
	const DynamicText& dyn,
	const glm::vec2& position,
	float scale,
	float z,
	const Math::Color& color)
{
	if (dyn.instances.empty())
		return;

	renderDynamic(dyn.instances, position, scale, z, color);
}

void TrueTypeFont::drawDynamic(
	std::string_view text,
	const glm::vec2& position,
	const Text::DrawParams& params)
{
	if (!font || text.empty())
		return;

	TextCacheKey key {
		std::string(text), params.scale, params.maxWidth, params.alignment, params.useMarkup
	};

	DynamicText* cached = dynamicTextCache.get(key);
	if (!cached) {
		DynamicText built = buildDynamic(text, params.maxWidth, params.alignment, params.useMarkup, params.scale);
		dynamicTextCache.put(key, std::move(built));
		cached = dynamicTextCache.get(key);
	}

	drawDynamic(*cached, position, params.scale, params.z, params.color);
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
		struct WordGlyph { const Glyph* glyph; float kern; size_t charIndex; };
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

			word.push_back({&g, static_cast<float>(kern), j});
			wordWidth += kern + g.advance;
			prev = wc;
		}

		if (maxWidth > 0.0f && cursorX > 0.0f && cursorX + wordWidth > maxWidth)
			newLine();

		for (const auto& wg : word) {
			cursorX += wg.kern;
			lines.back().glyphs.push_back({wg.glyph, cursorX, wg.charIndex});
			lines.back().width = cursorX + wg.glyph->size.x;
			cursorX += wg.glyph->advance;
		}

		i += word.size();
	}

	return lines;
}

} // namespace Blackthorn::Fonts