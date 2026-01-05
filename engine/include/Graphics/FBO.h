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
	GLsizei width;

	/// Height of the frame buffer in pixels.
	GLsizei height;

	/// Owned color attachment texture.
	std::unique_ptr<Texture> colorAttachment;

public:
	/**
	 * @brief Creates a frame buffer with a color texture attachment.
	 * @param w Width of the frame buffer in pixels.
	 * @param h Height of the frame buffer in pixels.
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
};

} // namespace Blackthorn::Graphics
