#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

#include "Containers/LRUCache.h"
#include "Core/Export.h"
#include "Fonts/Font.h"
#include "Fonts/TextCacheKey.h"
#include "Fonts/TextMarkup.h"
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
		Math::Color color = Math::Colors::White;
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
	void generateVertices(const Layout& layout, float scale, Text::Alignment alignment, std::vector<Vertex>& outVertices, const std::vector<TextStyle>* markup) const;

	/**
	 * @brief Parses a `.fnt`-style text metrics stream (the format produced
	 * by loadFromFile()'s @p metricsPath argument).
	 *
	 * Shared by loadFromFile() (reading a `std::ifstream`) and loadFromMemory()
	 * (reading a `std::istringstream` over an in-memory buffer). Populates
	 * @c glyphs, @c lineHeight, @c baseline, @c spaceWidth, and @c tabWidth.
	 *
	 * @param stream Text stream positioned at the start of the metrics data.
	 * @param sourceLabel Human-readable source identifier used in log messages
	 *                    only (e.g. a file path, or "<memory>").
	 * @return true on success. Malformed individual lines are logged and
	 *         skipped rather than failing the whole parse, matching the
	 *         original loadFromFile() behavior.
	 */
	bool parseMetricsText(std::istream& stream, const std::string& sourceLabel);

	/**
	 * @brief Parses a binary `.btf` stream (magic + version + metrics +
	 * embedded image + glyph table).
	 *
	 * Shared by loadFromBTFont() (reading a `std::ifstream`) and
	 * loadFromBTFontMemory() (reading a `std::istringstream` over an
	 * in-memory buffer). On success, replaces @c texture and populates
	 * @c glyphs and the layout metrics.
	 *
	 * @param stream Binary stream positioned at the start of the BTF data.
	 * @param sourceLabel Human-readable source identifier used in log messages
	 *                    only (e.g. a file path, or "<memory>").
	 * @return true on success.
	 */
	bool parseBTFontStream(std::istream& stream, const std::string& sourceLabel);

public:
	BitmapFont();

	BitmapFont(const BitmapFont&) = delete;
	BitmapFont& operator=(const BitmapFont&) = delete;

	BitmapFont(BitmapFont&& other) noexcept;
	BitmapFont& operator=(BitmapFont&& other) noexcept;

	bool loadFromFile(const std::filesystem::path& texturePath, const std::filesystem::path& metricsPath);
	bool loadFromBTFont(const std::filesystem::path& btfPath);

	/**
	 * @brief Loads a bitmap font from in-memory texture bytes and a
	 * `.fnt`-style text metrics buffer.
	 *
	 * @details
	 * Memory counterpart to loadFromFile(). Intended for asset-pipeline use,
	 * where both files have already been read into memory (e.g. decompressed
	 * from a `.btp` pack) rather than living on disk. @p textureData is an
	 * encoded image (PNG/etc.), decoded via SDL_image the same way
	 * loadFromBTFont() decodes its embedded image.
	 *
	 * @param textureData Pointer to the encoded texture image bytes.
	 * @param textureSize Size of @p textureData in bytes.
	 * @param metricsData Pointer to the `.fnt`-format metrics text bytes.
	 * @param metricsSize Size of @p metricsData in bytes.
	 * @return true on success.
	 */
	bool loadFromMemory(const U8* textureData, size_t textureSize, const U8* metricsData, size_t metricsSize);

	/**
	 * @brief Loads a bitmap font from an in-memory single-file `.btf` buffer.
	 *
	 * Memory counterpart to loadFromBTFont(). Intended for asset-pipeline use,
	 * where the `.btf` file has already been read into memory (e.g.
	 * decompressed from a `.btp` pack) rather than living on disk.
	 *
	 * @param data Pointer to the raw `.btf` file bytes.
	 * @param size Size of @p data in bytes.
	 * @return true on success.
	 */
	bool loadFromBTFontMemory(const U8* data, size_t size);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left, bool useMarkup = false) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left, bool useMarkup = false) override;

	Text::Metrics measure(std::string_view text, float scale = 1.0f, float maxWidth = 0.0f, bool useMarkup = false) override;

	float getLineHeight() const override { return lineHeight; }
	float getSpaceWidth() const { return spaceWidth; }
	float getTabWidth() const { return tabWidth; }
	bool isLoaded() const { return texture != nullptr; }
	const Graphics::Texture* getTexture() const { return texture.get(); }

	static void initializeShader();
	static void cleanupShader();
};

} // namespace Blackthorn::Fonts
