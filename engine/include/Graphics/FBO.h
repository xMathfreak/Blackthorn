#pragma once

#include <memory>

#include <glad/glad.h>

#include "Core/Export.h"
#include "Graphics/Texture.h"

namespace Blackthorn::Graphics {

/**
 * @brief RAII wrapper class for an OpenGL Frame Buffer Object with a color attachment.
 *
 * The frame buffer owns s single color texture attachment, which can be sampled after rendering.
 *
 * Copying is disallowed to enforce the unique ownership of the OpenGL resource
 * and its attachments. Move semantics are supported.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API FBO {
private:
	/// OpenGL Frame Buffer Object handle (0 if uninitialized)
	GLuint id = 0;

	/// Width of the frame buffer in pixels
	GLsizei width = 0;

	/// Height of the frame buffer in pixels.
	GLsizei height = 0;

	GLuint depthRBO = 0;

	/// Owned color attachment texture.
	std::unique_ptr<Texture> colorAttachment;

	/// Internal allocation, called by constructor and `resize()`;
	void allocate(GLsizei w, GLsizei h);

public:
	/**
	 * @brief Creates a frame buffer with a color texture attachment.
	 * @param w Width of the frame buffer in pixels.
	 * @param h Height of the frame buffer in pixels.
	 * @throws std::runtime_error if the frame buffer is incomplete after setup.
	 *
	 * Allocates a frame buffer object and attaches a 2D texture
	 * suitable for color rendering.
	 */
	FBO(GLsizei w, GLsizei h);

	/**
	 * @brief Destroys the frame buffer and releases owned resources.
	 */
	~FBO();

	/// Copy construction is disabled (unique ownership)
	FBO(const FBO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	FBO& operator=(const FBO&) = delete;

	/**
	 * @brief Move-constructs an FBO, transferring ownership.
	 * @param other FBO to move from.
	 */
	FBO(FBO&& other) noexcept;

	/**
	 * @brief Move-assigns an FBO, transferring ownership.
	 * @param other FBO to move from.
	 * @return Reference to this object.
	 */
	FBO& operator=(FBO&& other) noexcept;

	/**
	 * @brief Binds this frame buffer for rendering.
	 *
	 * All subsequent draw calls will render into this frame buffer
	 * until unbind() is called.
	 */
	void bind() const;

	/**
	 * @brief Binds the default frame buffer.
	 */
	static void unbind();

	/**
	 * @brief Resizes the frame buffer, reallocating all attachments
	 *
	 * Destroys the existing color texture and depth render buffer and
	 * creates new ones at the requested size. Any previously captured
	 * content is lost.
	 *
	 * @param w New width in pixels (must be > 0).
	 * @param h New height in pixels (must be > 0).
	 * @throws std::runtime_error if the new frame buffer is incomplete.
	 */
	void resize(GLsizei w, GLsizei h);

	/**
	 * @brief Destroys the frame buffer and its attachments.
	 *
	 * After calling this, the FBO becomes invalid.
	 */
	void destroy();

	/**
	 * @brief Returns the color attachment texture.
	 *
	 * The returned texture can be bound for sampling in later render passes.
	 */
	const Texture& getTexture() const;

	GLsizei getWidth() const { return width; }
	GLsizei getHeight() const { return height; }

	GLuint getID() const { return id; }
	bool isValid() const { return id != 0; }
};

} // namespace Blackthorn::Graphics
