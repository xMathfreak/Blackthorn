#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include "Fonts/Font.h"
#include "Graphics/Renderer.h"
#include "Graphics/Texture.h"
#include "Graphics/Shader.h"
#include "Graphics/EBO.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

namespace Blackthorn::Fonts {

class TrueTypeFont : public Font {
public:
	TrueTypeFont(Graphics::Renderer* renderer);
	~TrueTypeFont() override;

	TrueTypeFont(const TrueTypeFont&) = delete;
	TrueTypeFont& operator=(const TrueTypeFont&) = delete;

	bool loadFromFile(const std::string& filePath, int pointSize);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft) override;
	
	TextMetrics measure(std::string_view text, float scale, float maxWidth) const override;
	float getLineHeight() const override;

	void setStyle(TTF_FontStyleFlags style);
	void setOutline(int outline);
	void setHinting(TTF_HintingFlags hinting);
	void setKerning(bool enabled);

private:
	struct Glyph {
		glm::vec2 size;
		glm::vec2 bearing;
		float advance;
		glm::vec4 uv;
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec4 color;
		glm::vec2 texCoord;
	};

	struct GlyphBatch {
		GLsizei indices = 0;
		std::vector<Vertex> vertices;
	};

	struct LayoutGlyph {
		const Glyph* glyph;
		glm::vec2 pos;
	};

	struct LayoutLine {
		std::vector<LayoutGlyph> glyphs;
		float width = 0.0f;
	};

private:
	static std::shared_ptr<Graphics::Shader> shader;
	static Uint32 shaderRefCount;
	static void initShader();
	static void cleanShader();

	std::unique_ptr<Graphics::EBO> ebo;
	std::unique_ptr<Graphics::VAO> vao;
	std::unique_ptr<Graphics::VBO> vbo;

	static constexpr Uint32 MAX_GLYPHS = 1 << 11;
	static constexpr Uint32 MAX_VERTICES = MAX_GLYPHS * 4;
	static constexpr Uint32 MAX_INDICES = MAX_GLYPHS * 6;

	void initBuffers();

	Graphics::Renderer* renderer;
	TTF_Font* font;

	static constexpr int ATLAS_SIZE = 1024;
	std::unique_ptr<Graphics::Texture> atlas;
	glm::ivec2 atlasCursor;
	int atlasRowHeight;

	int fontSize;	
	float lineHeight;

	std::unordered_map<char32_t, Glyph> glyphCache;
	const Glyph& getGlyph(char32_t codePoint);

	void buildTextGeometry(const std::string& text, float x, float y, const glm::vec4& color, float scale, float maxWidth, std::vector<GlyphBatch>& batches);
	void renderBatches(const std::vector<GlyphBatch>& batches, float x, float y, const glm::vec4& color, float scale);
	std::vector<char32_t> utf8To32(const std::string& utf8) const;

	std::vector<LayoutLine> layoutText(const std::vector<char32_t>& text, float maxWidth);
};

} // namespace Blackthorn::Fonts
