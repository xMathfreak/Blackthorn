#pragma once

#include "Core/Export.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include "Graphics/EBO.h"
#include <Graphics/UBO.h>
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <array>
#include <memory>

namespace Blackthorn::Graphics {

/**
 * @brief Vertex format used by the 2D renderer.
 *
 * Represents a single vertex for batched quad rendering.
 */
struct Vertex2D {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec2 texCoords;
	float texIndex;
};

/**
 * @brief Batched 2D renderer built on OpenGL.
 *
 * The renderer internally manages vertex/index buffers, shaders,
 * textures, and uniform buffers. Users interact only through the
 * public drawing and state-setting API.
 *
 * Rendering follows a strict begin/end pattern:
 * - beginScene()
 * - draw calls
 * - endScene()
 *
 * Copying is disallowed; the renderer owns GPU resources and enforces
 * a single point of control.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API Renderer {
private:
	/// Maximum number of quads per batch
	static constexpr Uint32 MAX_QUADS = 2 << 13;

	/// Maximum number of vertices per batch
	static constexpr Uint32 MAX_VERTICES = MAX_QUADS * 4;

	/// Maximum number of indices per batch
	static constexpr Uint32 MAX_INDICES = MAX_QUADS * 6;

	/// Maximum number of texture slots per batch
	static constexpr Uint32 MAX_TEXTURE_SLOTS = 2 << 3;

	/// Index buffer for quad rendering
	std::unique_ptr<EBO> QuadEBO;

	/// Vertex array object for quad layout
	std::unique_ptr<VAO> QuadVAO;

	/// Vertex buffer for batched quad data
	std::unique_ptr<VBO> QuadVBO;

	/// Shader used for 2D rendering
	std::unique_ptr<Shader> shader;

	/**
	 * @brief Global uniform data shared across draw calls.
	 */
	struct GlobalData {
		/// Combined view-projection matrix
		alignas(16) glm::mat4 viewProjection;
	};

	/// Uniform buffer storing global rendering state
	std::unique_ptr<UBO<GlobalData>> globalUBO;

	/// Default 1x1 white texture used for untextured quads
	std::unique_ptr<Texture> whiteTexture;

	/// Current view bounds in world space
	SDL_FRect viewBounds{0, 0, 0, 0};

	/// Whether view frustum culling is enabled
	bool cullingEnabled = true;

	/// CPU-side vertex buffer for batching
	std::unique_ptr<Vertex2D[]> quadBuffer;

	/// Pointer to the current position in the batch buffer
	Vertex2D* quadBufferPtr = nullptr;

	/// Number of indices currently queued in the batch
	Uint32 quadIndexCount = 0;

	/// Active texture slots for the current batch
	std::array<const Texture*, MAX_TEXTURE_SLOTS> textureSlots;

	/// Next available texture slot index
	Uint32 textureSlotIndex = 1;

	/// Projection matrix
	glm::mat4 projectionMatrix;

	/// View matrix
	glm::mat4 viewMatrix;

	/**
	 * @brief Initializes the renderer shader.
	 */
	void initShader();

	/**
	 * @brief Initializes quad VAO, VBO, and EBO.
	 */
	void initQuadBuffers();

	/**
	 * @brief Creates the default white texture.
	 */
	void initWhiteTexture();

	/**
	 * @brief Begins a new rendering batch.
	 */
	void startBatch();

	/**
	 * @brief Ends the current batch and starts a new one.
	 */
	void nextBatch();

	/**
	 * @brief Flushes the current batch to the GPU.
	 */
	void flush();

	/**
	 * @brief Checks whether a rectangle is visible within the view bounds.
	 * @param rect Rectangle to test.
	 * @param rotation Optional rotation in radians.
	 */
	inline bool isVisible(const SDL_FRect& rect, float rotation = 0.0f) const;

	/**
	 * @brief Converts an SDL color to a glm::vec4.
	 */
	static inline constexpr glm::vec4 toGLMColor(const SDL_FColor& color);

	/**
	 * @brief Converts two floats to a glm::vec2.
	 */
	static inline constexpr glm::vec2 toGLMVec2(float x, float y);

	/**
	 * @brief Internal quad draw implementation.
	 */
	void draw(const SDL_FRect& rect, float z, float rotation, const SDL_FColor& color, const Texture* texture, const SDL_FRect* srcRect);
public:
	/**
	 * @brief Constructs the renderer and initializes GPU resources.
	 */
	Renderer();

	/**
	 * @brief Destroys the renderer and releases GPU resources.
	 */
	~Renderer();

	/// Copy construction is disabled
	Renderer(const Renderer&) = delete;

	/// Copy assignment is disabled
	Renderer& operator=(const Renderer&) = delete;

	/**
	 * @brief Begins a rendering scene.
	 *
	 * Must be called before issuing any draw calls.
	 */
	void beginScene();

	/**
	 * @brief Ends the current rendering scene and flushes pending draws.
	 */
	void endScene();

	/**
	 * @brief Sets an orthographic projection based on viewport size.
	 * @param width Viewport width in pixels.
	 * @param height Viewport height in pixels.
	 */
	void setProjection(int width, int height);

	/**
	 * @brief Sets the projection matrix explicitly.
	 */
	void setProjection(const glm::mat4& projection);

	/**
	 * @brief Sets the view matrix.
	 */
	void setView(const glm::mat4& view);

	/**
	 * @brief Enables or disables view frustum culling.
	 */
	void setCullingEnabled(bool enabled) { cullingEnabled = enabled; }

	/**
	 * @brief Checks whether culling is enabled.
	 */
	bool isCullingEnabled() const { return cullingEnabled; }

	/**
	 * @brief Returns the combined view-projection matrix.
	 */
	glm::mat4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }

	/**
	 * @brief Returns the current view matrix.
	 */
	const glm::mat4& getViewMatrix() const { return viewMatrix; }

	/**
	 * @brief Returns the current projection matrix.
	 */
	const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }

	/**
	 * @brief Returns the current view bounds.
	 */
	const SDL_FRect& getViewBounds() const { return viewBounds; }

	/**
	 * @brief Draws a colored quad.
	 * @param rect Destination rectangle.
	 * @param rotation Rotation in radians.
	 * @param z Z-depth value.
	 * @param color Quad color.
	 */
	void drawQuad(
		const SDL_FRect& rect,
		float rotation = 0.0f,
		float z = 0.0f,
		const SDL_FColor& color = { 1.0f, 1.0f, 1.0f, 1.0f }
	);

	/**
	 * @brief Draws a textured quad.
	 * @param texture Texture to draw.
	 * @param dest Destination rectangle.
	 * @param src Optional source rectangle within the texture.
	 * @param rotation Rotation in radians.
	 * @param z Z-depth value.
	 * @param tint Color tint applied to the texture.
	 */
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