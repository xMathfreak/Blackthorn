#pragma once

#include <array>
#include <memory>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Graphics/EBO.h"
#include "Graphics/FBO.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture.h"
#include "Graphics/Types.h"
#include "Graphics/UBO.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Math/Color.h"

namespace Blackthorn::Graphics {

/**
 * @brief Batched 2D renderer built on OpenGL.
 *
 * Rendering follows a strict begin/end pattern:
 *
 *   beginScene()  - binds the internal FBO, clears color + depth
 *   draw calls
 *   endScene()    - flushes batches, runs the fullscreen pass to the
 *                   default framebuffer
 *
 * The renderer owns an internal FBO with a color texture and depth
 * renderbuffer. At endScene(), a fullscreen pass draws the FBO color
 * attachment to the default framebuffer using an oversized triangle (no
 * vertex buffer - positions are generated from gl_VertexID in the shader).
 *
 * Post-processing is supported by swapping the screen shader via
 * setScreenShader(). The built-in screen shader supports grayscale, invert,
 * brightness, contrast, saturation, and gamma correction. Disable the
 * fullscreen pass entirely (falling back to glBlitFramebuffer) via
 * setPostProcessingEnabled(false).
 *
 * Copying is disallowed; the renderer owns GPU resources.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API Renderer {
private:
	/**
	 * @brief Vertex format used by the 2D renderer.
	 *
	 * Represents a single vertex for batched quad rendering.
	 */
	struct Vertex {
		glm::vec3 position;
		glm::vec4 color;
		glm::vec2 texCoords;
		float texIndex;
	};

private:
	/// Default number of quads per batch - overridable at construction
	static constexpr U32 DEFAULT_MAX_QUADS = 1 << 12;

	/// Maximum number of texture slots per batch
	static constexpr U32 MAX_TEXTURE_SLOTS = 2 << 3;

	/// Runtime batch limits (set from maxQuads passed to constructor)
	U32 MAX_QUADS = DEFAULT_MAX_QUADS;
	U32 MAX_VERTICES = DEFAULT_MAX_QUADS * 4;
	U32 MAX_INDICES = DEFAULT_MAX_QUADS * 6;

	/// Index buffer for quad rendering
	std::unique_ptr<EBO> QuadEBO;

	/// Vertex array object for quad layout
	std::unique_ptr<VAO> QuadVAO;

	/// Vertex buffer for batched quad data
	std::unique_ptr<VBO> QuadVBO;

	/// Shader used for 2D rendering
	std::unique_ptr<Shader> shader;

	/// Shader for Post Processing effects
	std::unique_ptr<Shader> screenShader;

	std::unique_ptr<VAO> screenVAO;

	Shader* activeScreenShader = nullptr;
	bool postProcessingEnabled = true;

	std::unique_ptr<FBO> fbo;

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

	/// CPU-side vertex buffer - written each batch, uploaded via glBufferSubData
	std::unique_ptr<Vertex[]> quadBuffer;

	/// Pointer to the current write position within quadBuffer
	Vertex* quadBufferPtr = nullptr;

	/// Number of indices currently queued in the batch
	U32 quadIndexCount = 0;

	/// Active texture slots for the current batch
	std::array<const Texture*, MAX_TEXTURE_SLOTS> textureSlots;

	/// Next available texture slot index
	U32 textureSlotIndex = 1;

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

	void initScreenPass();

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

	U32 findOrAddTexture(const Texture* texture);

	/**
	 * @brief Internal quad draw implementation.
	 */
	void draw(const SDL_FRect& rect, float z, float rotation, const Math::Color& color, const Texture* texture, const SDL_FRect* srcRect);

	/// Runs the fullscreen pass, drawing the FBO to the default frame buffer
	void presentToScreen();
	/// Background clear color applied to the FBO each frame
	Math::Color clearColor = Math::Colors::Black;

public:
	/**
	 * @brief Constructs the renderer and initializes GPU resources.
	 * @param maxQuads Maximum quads per batch. Defaults to 4096.
	 */
	explicit Renderer(U32 maxQuads = DEFAULT_MAX_QUADS);

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
	 * @brief Sets the clear color used at the start of each scene.
	 */
	void setClearColor(float r, float g, float b, float a = 1.0f) {
		clearColor = { r, g, b, a };
	}

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
	 * @brief Gets the global uniform buffer.
	 */
	const UBO<GlobalData>& getUniformBuffer() const { return *globalUBO; }

	/**
	 * @brief Enable or disable the fullscreen shader pass.
	 *
	 * When enabled (default), endScene() draws the FBO through the active
	 * screen shader. When disabled, endScene() falls back to a raw
	 * glBlitFramebuffer call - marginally faster but no shader effects.
	 */
	void setPostProcessingEnabled(bool enabled);
	bool isPostProcessingEnabled() const { return postProcessingEnabled; }

	/**
	 * @brief Override the screen shader used for the fullscreen pass.
	 *
	 * Pass nullptr to restore the built-in passthrough / post-process shader.
	 * The renderer does NOT take ownership - the caller must keep the shader
	 * alive for as long as it is active.
	 *
	 * @param customShader Shader to use, or nullptr to reset to built-in.
	 */
	void setScreenShader(Shader* customShader);

	/**
	 * @brief Returns the built-in screen shader for direct uniform access.
	 * Use this to toggle built-in effects (grayscale, invert, etc.) without
	 * supplying a custom shader.
	 */
	Shader& getScreenShader() const { return *screenShader; }

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
		const Math::Color& color = Math::Colors::White
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
		const Math::Color& tint = Math::Colors::White
	);

	/**
	 * @brief Draws a nine slice texture.
	 * @param texture The texture to draw
	 * @param dest Destination rectangle.
	 * @param sliceMargins The slice margins separating the 9 quadrants.
	 * @param z Z-depth value.
	 * @param tint Color tint applied to the texture.
	 */
	void drawNineSlice(
		const Texture& texture,
		const SDL_FRect& dest,
		const SliceMargins& sliceMargins,
		float z = 0.0f,
		const Math::Color& tint = Math::Colors::White
	);
};

} // namespace Blackthorn::Graphics