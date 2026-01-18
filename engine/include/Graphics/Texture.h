#pragma once

#include <string>

#include <glad/glad.h>
#include <SDL3/SDL.h>

#include "Core/Export.h"

namespace Blackthorn::Graphics {

/**
 * @brief Texture filtering modes.
 */
enum class TextureFilter {
	/// Nearest-neighbor sampling
	Nearest,
	/// Linear Filtering
	Linear
};

/**
 * @brief Texture wrapping modes.
 */
enum class TextureWrap {
	/// Repeat texture coordinates
	Repeat,
	/// Mirrored repeat
	MirroredRepeat,
	/// Clamp to edge
	ClampToEdge,
	/// Clamp to border
	ClampToBorder
};

/**
 * @brief Describes texture sampling and wrapping behavior.
 *
 * These parameters are applied when the texture is created or loaded.
 */
struct TextureParams {
	/// Minification filter
	TextureFilter minFilter = TextureFilter::Nearest;
	/// Magnification filter
	TextureFilter magFilter = TextureFilter::Nearest;
	/// Horizontal wrapping mode
	TextureWrap wrapS = TextureWrap::Repeat;
	/// Vertical wrapping mode
	TextureWrap wrapT = TextureWrap::Repeat;
	/// Whether to generate mipmaps
	bool generateMipmaps = false;
};

/**
 * @brief RAII wrapper for a 2D OpenGL texture.
 *
 * The texture owns the OpenGL texture object and deletes it automatically
 * on destruction. Copying is disallowed; move semantics are supported.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API Texture {
private:
	/// OpenGL texture object handle (0 if uninitialized)
	GLuint id = 0;

	/// Texture width in pixels
	int width = 0;

	/// Texture height in pixels
	int height = 0;

	/// Number of color channels
	int channels = 0;

	/// Texture sampling and wrapping parameters
	TextureParams params;

	/**
	 * @brief Applies texture parameters to the currently bound texture.
	 */
	void applyParams();

	/**
	 * @brief Converts an engine texture filter to an OpenGL enum.
	 */
	static GLenum toGLFilter(TextureFilter filter);

	/**
	 * @brief Converts an engine texture wrap mode to an OpenGL enum.
	 */
	static GLenum toGLWrap(TextureWrap wrap);

public:
	/**
	 * @brief Constructs an empty texture.
	 */
	Texture() = default;

	/**
	 * @brief Loads a texture from a file.
	 * @param path Path to the image file.
	 * @param parameters Texture sampling and wrapping parameters.
	 */
	explicit Texture(const std::string& path, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Creates a texture from raw pixel data.
	 * @param width Texture width in pixels.
	 * @param height Texture height in pixels.
	 * @param channels Number of color channels.
	 * @param data Pointer to pixel data.
	 * @param parameters Texture sampling and wrapping parameters.
	 */
	Texture(int width, int height, int channels, const void* data, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Creates an empty texture with allocated storage.
	 * @param width Texture width in pixels.
	 * @param height Texture height in pixels.
	 * @param channels Number of color channels.
	 * @param parameters Texture sampling and wrapping parameters.
	 */
	Texture(int width, int height, int channels = 4, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Destroys the texture and releases the OpenGL resource.
	 */
	~Texture();

	/// Copy construction is disabled (unique ownership)
	Texture(const Texture&) = delete;

	/// Copy assignment is disabled (unique ownership)
	Texture& operator=(const Texture&) = delete;

	/**
	 * @brief Move-constructs a texture, transferring ownership.
	 * @param other Texture to move from.
	 */
	Texture(Texture&& other) noexcept;

	/**
	 * @brief Move-assigns a texture, transferring ownership.
	 * @param other Texture to move from.
	 * @return Reference to this object.
	 */
	Texture& operator=(Texture&& other) noexcept;

	/**
	 * @brief Loads texture data from a file.
	 * @param path Path to the image file.
	 * @param parameters Texture sampling and wrapping parameters.
	 * @return True on success, false otherwise.
	 */
	bool loadFromFile(const std::string& path, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Loads texture data from an SDL Surface.
	 * @param surface The SDL_Surface pointer.
	 * @param parameters Texture sampling and wrapping parameters.
	 * @return True on success, false otherwise.
	 * @note This method does not destroy the SDL_Surface passed to it.
	 */
	bool loadFromSurface(SDL_Surface* surface, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Loads texture data from memory.
	 * @param w Width in pixels.
	 * @param h Height in pixels.
	 * @param ch Number of channels.
	 * @param data Pointer to pixel data.
	 * @param parameters Texture sampling and wrapping parameters.
	 * @return True on success, false otherwise.
	 */
	bool loadFromMemory(int w, int h, int ch, const void* data, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Creates an empty texture with allocated storage.
	 * @param width Texture width in pixels.
	 * @param height Texture height in pixels.
	 * @param channels Number of channels.
	 * @param parameters Texture sampling and wrapping parameters.
	 * @return True on success, false otherwise.
	 */
	bool create(int width, int height, int channels = 4, const TextureParams& parameters = TextureParams());

	/**
	 * @brief Destroys the texture and releases its OpenGL resource.
	 */
	void destroy();

	/**
	 * @brief Binds the texture to a texture unit.
	 * @param slot Texture unit index.
	 */
	void bind(GLuint slot = 0) const;

	/**
	 * @brief Unbinds any texture from a texture unit.
	 * @param slot Texture unit index.
	 */
	static void unbind(GLuint slot = 0);

	/**
	 * @brief Updates a sub-region of the texture.
	 * @param x X offset in pixels.
	 * @param y Y offset in pixels.
	 * @param width Region width in pixels.
	 * @param height Region height in pixels.
	 * @param data Pointer to new pixel data.
	 */
	void updateRegion(int x, int y, int width, int height, const void* data);

	/**
	 * @brief Checks whether the texture has been created.
	 */
	bool isValid() const noexcept { return id != 0; }

	/**
	 * @brief Returns the OpenGL texture handle.
	 */
	GLuint getID() const noexcept { return id; }

	/**
	 * @brief Returns the texture width in pixels.
	 */
	int getWidth() const noexcept { return width; }

	/**
	 * @brief Returns the texture height in pixels.
	 */
	int getHeight() const noexcept { return height; }

	/**
	 * @brief Returns the number of color channels.
	 */
	int getChannels() const noexcept { return channels; }

	/**
	 * @brief Returns the texture parameters.
	 */
	const TextureParams& getParams() const noexcept { return params; }

	/**
	 * @brief Creates a default texture.
	 *
	 * Used as a fallback when texture loading fails.
	 */
	static Texture createDefault();
};

} // namespace Blackthorn::Graphics