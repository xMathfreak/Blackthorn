#pragma once

#include <memory>
#include <string>

#include "Core/Export.h"
#include "Fonts/Font.h"
#include "Fonts/TextCacheKey.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Utils/LRUCache.h"

namespace Blackthorn::Fonts {

class BLACKTHORN_API BitmapFont : public Font {
private:
	struct Glyph {
		SDL_FRect rect;
		Sint16 xOffset;
		Sint16 yOffset;
		Sint16 xAdvance;
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec2 texCoord;
	};

	struct CachedText {
		Graphics::VAO vao;
		Graphics::VBO vbo;
		size_t vertexCount = 0;
		float width = 0;
		float height = 0;
	};

private:
	static std::shared_ptr<Graphics::Shader> shader;
	Graphics::Renderer* renderer = nullptr;

	static constexpr Uint32 MAX_TEXT_GLYPHS = 2048;
	static constexpr Uint32 MAX_VERTICES = MAX_TEXT_GLYPHS * 4;

	std::unique_ptr<Graphics::VAO> vao;
	std::unique_ptr<Graphics::VBO> vbo;

	void initBuffers();

	std::unique_ptr<Graphics::Texture> texture;
	std::unordered_map<Uint32, Glyph> glyphs;
	
	float baseline = 0.0f;
	float lineHeight = 0.0f;
	float spaceWidth = 0.0f;
	float tabWidth = 0.0f;

	static constexpr Uint32 MAX_CACHED_TEXT = 128;
	Utils::LRUCache<TextCacheKey, CachedText> cache{MAX_CACHED_TEXT};

	mutable std::vector<std::string_view> lineBuffer;
	mutable std::vector<float> lineWidthBuffer;
	mutable std::vector<Vertex> vertexBuffer;

	void wrapText(std::string_view text, float scale, float maxWidth, std::vector<std::string_view>& outLines) const;
	float computeLineWidth(std::string_view line, float scale) const;
	TextMetrics computeMetrics(std::string_view text, float scale, float maxWidth) const;
	void generateVertices(std::string_view text, float scale, float maxWidth, TextAlign alignment, std::vector<Vertex>& outVertices) const; 

public:
	BitmapFont(Graphics::Renderer* ren);

	BitmapFont(const BitmapFont&) = delete;
	BitmapFont& operator=(const BitmapFont&) = delete;

	BitmapFont(BitmapFont&& other) noexcept;
	BitmapFont& operator=(BitmapFont&& other) noexcept;

	bool loadFromFile(const std::string& texturePath, const std::string& metricsPath);
	bool loadFromBMFont(const std::string& bmfPath);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::Left) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::Left) override;

	TextMetrics measure(std::string_view text, float scale = 1.0f, float maxWidth = 0.0f) override;

	float getLineHeight() const override { return lineHeight; }
	float getSpaceWidth() const { return spaceWidth; }
	float getTabWidth() const { return tabWidth; }
	bool isLoaded() const { return texture != nullptr; }
	const Graphics::Texture* getTexture() const { return texture.get(); }

	static void initializeShader();
	static void cleanupShader();
};

} // namespace Blackthorn::Fonts
