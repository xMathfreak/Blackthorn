#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "Containers/LRUCache.h"
#include "Core/Export.h"
#include "Fonts/Font.h"
#include "Fonts/TextCacheKey.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Math/Color.h"

namespace Blackthorn::Fonts {

class BLACKTHORN_API BitmapFont : public Font {
private:
	struct Glyph {
		SDL_FRect rect;
		I16 xOffset;
		I16 yOffset;
		I16 xAdvance;
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec2 texCoord;
	};

	struct CachedText {
		Graphics::VAO vao;
		Graphics::VBO vbo;
		size_t vertexCount = 0;
	};

	struct Layout {
		std::vector<std::string_view> lines;
		std::vector<float> lineWidths;
		float totalWidth = 0.0f;
		float totalHeight = 0.0f;
	};

private:
	static std::shared_ptr<Graphics::Shader> shader;

	U32 MAX_TEXT_GLYPHS;
	U32 MAX_VERTICES;

	std::unique_ptr<Graphics::VAO> vao;
	std::unique_ptr<Graphics::VBO> vbo;

	void initBuffers();

	std::unique_ptr<Graphics::Texture> texture;
	std::unordered_map<U32, Glyph> glyphs;

	float baseline = 0.0f;
	float lineHeight = 0.0f;
	float spaceWidth = 0.0f;
	float tabWidth = 0.0f;

	Containers::LRUCache<TextCacheKey, CachedText> cache;

	mutable std::vector<std::string_view> lineBuffer;
	mutable std::vector<float> lineWidthBuffer;
	mutable std::vector<Vertex> vertexBuffer;

	Layout buildLayout(std::string_view text, float scale, float maxWidth) const;
	void generateVertices(const Layout& layout, float scale, Text::Alignment alignment, std::vector<Vertex>& outVertices) const;

public:
	BitmapFont();

	BitmapFont(const BitmapFont&) = delete;
	BitmapFont& operator=(const BitmapFont&) = delete;

	BitmapFont(BitmapFont&& other) noexcept;
	BitmapFont& operator=(BitmapFont&& other) noexcept;

	bool loadFromFile(const std::filesystem::path& texturePath, const std::filesystem::path& metricsPath);
	bool loadFromBMFont(const std::filesystem::path& bmfPath);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left) override;

	Text::Metrics measure(std::string_view text, float scale = 1.0f, float maxWidth = 0.0f) override;

	float getLineHeight() const override { return lineHeight; }
	float getSpaceWidth() const { return spaceWidth; }
	float getTabWidth() const { return tabWidth; }
	bool isLoaded() const { return texture != nullptr; }
	const Graphics::Texture* getTexture() const { return texture.get(); }

	static void initializeShader();
	static void cleanupShader();
};

} // namespace Blackthorn::Fonts
