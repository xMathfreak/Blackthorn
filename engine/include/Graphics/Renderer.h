#pragma once

#include "Core/Export.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include <Graphics/GlobalUBO.h>
#include "Graphics/EBO.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <array>
#include <memory>

namespace Blackthorn::Graphics {

struct Vertex2D {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texCoords;
	float texIndex;
};

class BLACKTHORN_API Renderer {
private:
	static constexpr Uint32 MAX_QUADS = 2 << 13;
	static constexpr Uint32 MAX_VERTICES = MAX_QUADS * 4;
	static constexpr Uint32 MAX_INDICES = MAX_QUADS * 6;
	static constexpr Uint32 MAX_TEXTURE_SLOTS = 2 << 3;

	std::unique_ptr<EBO> QuadEBO;
	std::unique_ptr<VAO> QuadVAO;
	std::unique_ptr<VBO> QuadVBO;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<GlobalUBO> globalUBO;

	std::unique_ptr<Texture> whiteTexture;

	SDL_FRect viewBounds{0, 0, 0, 0};
	bool cullingEnabled = true;

	Vertex2D* quadBuffer = nullptr;
	Vertex2D* quadBufferPtr = nullptr;
	Uint32 quadIndexCount = 0;

	std::array<const Texture*, MAX_TEXTURE_SLOTS> textureSlots;
	Uint32 textureSlotIndex = 1;

	glm::mat4 viewProjectionMatrix;

	void initShader();
	void initQuadBuffers();
	void initWhiteTexture();

	void startBatch();
	void nextBatch();
	void flush();

	inline bool isVisible(const SDL_FRect& rect, float rotation = 0.0f) const {
		if (!cullingEnabled)
			return true;

		if (rotation == 0.0f)
			return SDL_HasRectIntersectionFloat(&rect, &viewBounds);

		float cx = rect.x + rect.w * 0.5f;
		float cy = rect.y + rect.h * 0.5f;

		float radius = std::sqrt(rect.w * rect.w + rect.h * rect.h) * 0.5f;

		return (
			cx + radius >= viewBounds.x &&
			cx - radius <= viewBounds.x + viewBounds.w &&
			cy + radius >= viewBounds.y &&
			cy - radius <= viewBounds.y + viewBounds.h
		);
	}

	static glm::vec4 toGLMColor(const SDL_FColor& color);
	static glm::vec2 toGLMVec2(float x, float y);

	void draw(const SDL_FRect& rect, float z, float rotation, const SDL_FColor& color, const Texture* texture, const SDL_FRect* srcRect);
public:
	Renderer();
	~Renderer();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	void beginScene(const glm::mat4& projectionMatrix);
	void endScene();

	void setViewBounds(float left, float bottom, float width, float height) {
		viewBounds = { left, bottom, width, height };
	}

	void setCullingEnabled(bool enabled) { cullingEnabled = enabled; }

	void drawQuad(
		const SDL_FRect& rect,
		float rotation = 0.0f,
		float z = 0.0f,
		const SDL_FColor& color = { 1.0f, 1.0f, 1.0f, 1.0f }
	);

	void drawTexture(
		const Texture& texture,
		const SDL_FRect& dest,
		const SDL_FRect* src = nullptr,
		float rotation = 0.0f,
		float z = 0.0f,
		const SDL_FColor& tint = { 1.0f, 1.0f, 1.0f, 1.0f }
	);
};

} // namespace Blackthorn::Graphics