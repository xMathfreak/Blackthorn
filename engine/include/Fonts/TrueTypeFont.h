#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include "Core/Export.h"
#include "Fonts/Font.h"
#include "Graphics/Renderer.h"
#include "Graphics/Texture.h"
#include "Graphics/Shader.h"
#include "Graphics/EBO.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

namespace Blackthorn::Fonts {

class BLACKTHORN_API TrueTypeFont : public Font {
public:
	TrueTypeFont(Graphics::Renderer* renderer);
	~TrueTypeFont() override;

	TrueTypeFont(const TrueTypeFont&) = delete;
	TrueTypeFont& operator=(const TrueTypeFont&) = delete;

	bool loadFromFile(const std::string& filePath, int pointSize);

	void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::Left) override;
	void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::Left) override;
	
	TextMetrics measure(std::string_view text, float scale, float maxWidth) override;
	float getLineHeight() const override;

	void setStyle(TTF_FontStyleFlags style);
	void setOutline(int outline);
	void setHinting(TTF_HintingFlags hinting);
	void setKerning(bool enabled);

private:
	struct Glyph {
		glm::vec2 size;
		glm::vec2 bearing;
		glm::vec4 uv;
		float advance;
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec2 texCoord;
		glm::vec4 color;
	};

	struct LayoutGlyph {
		const Glyph* glyph;
		glm::vec2 pos;
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
	void initShader();

	static constexpr Uint32 MAX_TEXT_GLYPHS = 2048;
	static constexpr Uint32 MAX_VERTICES = MAX_TEXT_GLYPHS * 4;
	static constexpr Uint32 MAX_INDICES = MAX_TEXT_GLYPHS * 6;

	std::unique_ptr<Graphics::EBO> ebo;
	std::unique_ptr<Graphics::VAO> vao;
	std::unique_ptr<Graphics::VBO> vbo;
	void initBuffers();

	Graphics::Renderer* renderer;
	TTF_Font* font = nullptr;

	static constexpr int ATLAS_SIZE = 1024;
	std::unique_ptr<Graphics::Texture> atlas;
	glm::ivec2 atlasCursor{0, 0};
	int atlasRowHeight = 0;

	float lineHeight = 0.0f;

	static constexpr Uint32 TAB_SPACES = 4;

	std::unordered_map<char32_t, Glyph> glyphCache;
	std::unordered_map<std::string, CachedText> textCache;

private:
	const Glyph& getGlyph(char32_t codePoint);
	
	void buildTextGeometry(std::string_view text, float maxWidth, const glm::vec4& color, TextAlign alignment,std::vector<Vertex>& outVertices, GLsizei& outIndexCount);
	void render(const std::vector<Vertex>& vertices, GLsizei indexCount, const glm::vec2& position, float scale, const glm::vec4& color);
	
	std::vector<char32_t> utf8To32(std::string_view utf8) const;

	std::vector<LayoutLine> layoutText(const std::vector<char32_t>& text, float maxWidth);
};

} // namespace Blackthorn::Fonts
