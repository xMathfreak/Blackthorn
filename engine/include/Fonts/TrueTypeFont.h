#pragma once

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include "Containers/LRUCache.h"
#include "Core/Export.h"
#include "Fonts/Font.h"
#include "Fonts/TextCacheKey.h"
#include "Graphics/Texture.h"
#include "Graphics/Shader.h"
#include "Graphics/EBO.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

namespace Blackthorn::Fonts {

class BLACKTHORN_API TrueTypeFont : public Font {
public:
	TrueTypeFont();
	~TrueTypeFont() override;

	TrueTypeFont(const TrueTypeFont&) = delete;
	TrueTypeFont& operator=(const TrueTypeFont&) = delete;

	bool loadFromFile(const std::filesystem::path& filePath, int pointSize);

	/**
	 * @brief Loads a TrueType/OpenType font from an in-memory buffer.
	 *
	 * @details
	 * Intended for asset-pipeline use, where font bytes have already been
	 * read into memory (e.g. decompressed from a `.btp` pack) rather than
	 * living on disk. Behaves like `loadFromFile()` otherwise - SDF and
	 * kerning are enabled and the glyph atlas is (re)initialized.
	 *
	 * @note SDL_ttf/FreeType keep referencing the font's raw bytes for the
	 * lifetime of the underlying `TTF_Font`, so @p data is copied into an
	 * internally-owned buffer; the caller's buffer does not need to outlive
	 * this call.
	 *
	 * @param data Pointer to the raw font file bytes (.ttf/.otf).
	 * @param size Size of @p data in bytes.
	 * @param pointSize Point size to load the font at.
	 * @return true on success.
	 */
	bool loadFromMemory(const U8* data, size_t size, int pointSize);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left) override;

	Text::Metrics measure(std::string_view text, float scale, float maxWidth) override;
	float getLineHeight() const override;

	void setStyle(TTF_FontStyleFlags style);
	void setOutline(int outline);
	void setHinting(TTF_HintingFlags hinting);
	void setKerning(bool enabled);

	static void initializeShader();
	static void cleanupShader();

private:
	struct Glyph {
		glm::vec2 size;
		glm::vec4 uv;
		float advance;
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec2 texCoord;
	};

	struct LayoutGlyph {
		const Glyph* glyph;
		float xPos;
	};

	struct LayoutLine {
		std::vector<LayoutGlyph> glyphs;
		float width = 0.0f;
	};

	struct CachedText {
		std::vector<Vertex> vertices;
		GLsizei indexCount = 0;
	};

private:
	static std::shared_ptr<Graphics::Shader> shader;

	U32 MAX_TEXT_GLYPHS;
	U32 MAX_VERTICES;
	U32 MAX_INDICES;

	std::unique_ptr<Graphics::EBO> ebo;
	std::unique_ptr<Graphics::VAO> vao;
	std::unique_ptr<Graphics::VBO> vbo;
	void initBuffers();

	TTF_Font* font = nullptr;

	std::vector<U8> fontDataBuffer;

	std::unique_ptr<Graphics::Texture> atlas;
	glm::ivec2 atlasCursor{0, 0};
	int atlasRowHeight = 0;

	float lineHeight = 0.0f;

	U32 TAB_SPACES;

	std::unordered_map<char32_t, Glyph> glyphCache;

	std::vector<U8> reuseBuffer;
	Containers::LRUCache<TextCacheKey, CachedText> textCache;

private:
	/**
	 * @brief Allocates/resets the glyph atlas texture and cache.
	 *
	 * Shared setup performed after `font` has been successfully opened by
	 * either loadFromFile() or loadFromMemory().
	 */
	void initializeAtlas();

	const Glyph& getGlyph(char32_t codePoint);

	void generateVertices(std::string_view text, float maxWidth, Text::Alignment alignment,std::vector<Vertex>& outVertices, GLsizei& outIndexCount);
	void render(const std::vector<Vertex>& vertices, GLsizei indexCount, const glm::vec2& position, float scale, float z, const Math::Color& color);

	std::vector<char32_t> utf8To32(std::string_view utf8) const;

	std::vector<LayoutLine> layoutText(const std::vector<char32_t>& text, float maxWidth);
};

} // namespace Blackthorn::Fonts
