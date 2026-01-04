#pragma once

#include "Core/Export.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

#include <deque>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace Blackthorn {

enum class TextAlign {
	TopLeft,
	TopCenter,
	TopRight,
	CenterLeft,
	Center,
	CenterRight,
	BottomLeft,
	BottomCenter,
	BottomRight
};

struct Glyph {
	SDL_FRect rect;
	Sint16 xOffset;
	Sint16 yOffset;
	Sint16 xAdvance;
};

struct TextMetrics {
	float width;
	float height;
	size_t lineCount;
};

struct TextVertex {
	glm::vec3 position;
	glm::vec2 texCoord;
	glm::vec4 color;
};

namespace Internal {

struct TextCacheKey {
	std::string text;
	float scale;
	float maxWidth;
	SDL_FColor color;
	TextAlign alignment;

	size_t hash() const noexcept;

	bool operator==(const TextCacheKey& other) const noexcept {
		return text == other.text
			&& scale == other.scale
			&& maxWidth == other.maxWidth
			&& alignment == other.alignment
			&& memcmp(&color, &other.color, sizeof(SDL_FColor)) == 0;
	}
};

} // namespace Internal

} // namespace Blackthorn

namespace std {
	template <>
	struct hash<Blackthorn::Internal::TextCacheKey> {
		size_t operator()(const Blackthorn::Internal::TextCacheKey& key) const noexcept {
			return key.hash();
		}
	};
} // namespace std

namespace Blackthorn {

class BLACKTHORN_API BitmapFont {
private:
	static std::shared_ptr<Graphics::Shader> fontShader;
	static Uint32 fontShaderRefCount;
	Graphics::Renderer* renderer = nullptr;

	std::unique_ptr<Graphics::Texture> texture;
	std::unordered_map<Uint32, Glyph> glyphs;

	float lineHeight = 0.0f;
	float spaceWidth = 0.0f;
	float tabWidth = 0.0f;

	struct CachedText {
		Graphics::VAO vao;
		Graphics::VBO vbo;
		size_t vertexCount = 0;
		float width = 0;
		float height = 0;
	};

	std::unordered_map<Internal::TextCacheKey, CachedText> cache;
	std::deque<Internal::TextCacheKey> cacheOrder;
	size_t maxCacheSize = 128;

	mutable std::vector<std::string_view> lineBuffer;
	mutable std::vector<float> lineWidthBuffer;
	mutable std::vector<TextVertex> vertexBuffer;

	void wrapText(std::string_view text, float scale, float maxWidth, std::vector<std::string_view>& outLines) const;
	float computeLineWidth(std::string_view line, float scale) const;
	TextMetrics computeMetrics(std::string_view text, float scale, float maxWidth) const;
	void generateVertices(std::string_view text, float x, float y, float scale, float maxWidth, const SDL_FColor& color, TextAlign alignment, std::vector<TextVertex>& outVertices, bool flipY = false) const;

	CachedText& getCache(const Internal::TextCacheKey& key);
	void evictOldestCache();
	void clearCache(); 

public:
	BitmapFont(Graphics::Renderer* ren)
		: renderer(ren)
	{
		if (fontShaderRefCount == 0)
			initializeShader();

		fontShaderRefCount++;
	}

	~BitmapFont();

	BitmapFont(const BitmapFont&) = delete;
	BitmapFont& operator=(const BitmapFont&) = delete;

	BitmapFont(BitmapFont&& other) noexcept;
	BitmapFont& operator=(BitmapFont&& other) noexcept;

	bool loadFromFile(const std::string& texturePath, const std::string& metricsPath);
	bool loadFromBMFont(const std::string& bmfPath);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft);
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft);

	TextMetrics measure(std::string_view text, float scale = 1.0f, float maxWidth = 0.0f) const;

	void setCacheSize(size_t maxSize) { maxCacheSize = maxSize; }
	size_t getCacheSize() const { return cache.size(); }
	void invalidateCache() { clearCache(); }

	float getLineHeight() const { return lineHeight; }
	float getSpaceWidth() const { return spaceWidth; }
	float getTabWidth() const { return tabWidth; }
	bool isLoaded() const { return texture != nullptr; }
	const Graphics::Texture* getTexture() const { return texture.get(); }

	static void initializeShader();
	static void cleanupShader();
};

} // namespace Blackthorn
