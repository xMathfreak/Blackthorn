#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <vector>

namespace Blackthorn::Graphics {

/**
 * @brief RAII Wrapper for an OpenGL Vertex Buffer Object (GL_ARRAY_BUFFER)
 * 
 * Copying is disallowed to enforce unique ownership of the OpenGL resource.
 * Move semantics are supported to allow safe transfer of ownership.
 * 
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API VBO {
private:
	/// OpenGL buffer object handle (0 if uninitialized)
	GLuint id = 0;

	/// Size of the buffer in bytes
	size_t size = 0;

public:
	/**
	 * @brief Constructs an empty VBO without creating the OpenGL buffer
	 * 
	 * Call create() explicitly or use the constructor taking createNow = true 
	 * before uploading data.
	 */
	VBO() = default;

	/**
	 * @brief Constructs a VBO and optionally creates the OpenGL buffer.
	 * @param createNow If true, create() is called immediately.
	 */
	explicit VBO(bool createNow);

	/**
	 * @brief Destroys the VBO and releases the OpenGL buffer.
	 * 
	 * Safe to call even if the buffer was never created.
	 */
	~VBO();

	/// Copy construction is disabled (unique ownership)
	VBO(const VBO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	VBO& operator=(const VBO&) = delete;

	/**
	 * @brief Move-constructs a VBO, transferring ownership.
	 * @param other VBO to move from.
	 * 
	 * The moved-from object is left in an invalid but destructible state. 
	 */
	VBO(VBO&& other) noexcept;

	/**
	 * @brief Move-assigns a VBO, transferring ownership.
	 * @param other VBO to move from.
	 * @return Reference to this object.
	 */
	VBO& operator=(VBO&& other) noexcept;

	/**
	 * @brief Creates the OpenGL buffer object.
	 * 
	 * If the buffer already exist, this function has no effect.
	 */
	void create();

	/**
	 * @brief Destroys the OpenGL buffer object.
	 * 
	 * After calling this, isValid() will return false.
	 */
	void destroy();

	/**
	 * @brief Binds this VBO to GL_ARRAY_BUFFER.
	 * 
	 * @pre The buffer must be valid.
	 */
	void bind() const;

	/**
	 * @brief Unbinds any VBO from GL_ARRAY_BUFFER.
	 */
	static void unbind();

	/**
	 * @brief Allocates and uploads data to the buffer.
	 * @tparam T Element type of the data.
	 * @param data SOurce data to upload.
	 * @param usage OpenGL usage hint (e.g. GL_STATIC_DRAW).
	 * 
	 * If the buffer has not yet been created, it will be created automatically.
	 * Any existing buffer storage is replaced.
	 */
	template <typename T>
	void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogWarn(
					SDL_LOG_CATEGORY_RENDER,
					"Attempting to set data on uninitialized VBO"
				);
			#endif

			create();
		}

		bind();
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), usage);
		size = data.size() * sizeof(T);

		#ifdef BLACKTHORN_DEBUG
			SDL_Log(
				"VBO %u: Uploaded %zu bytes (%zu elements of size %zu)",
				id, size, data.size(), sizeof(T)
			);
		#endif
	}

	/**
	 * @brief Allocates and uploads raw data to the buffer.
	 * @param data Pointer to the source data.
	 * @param sizeInBytes Size of the data in bytes.
	 * @param usage OpenGL usage hint.
	 * 
	 * Replaces any existing buffer storage.
	 */
	void setData(const void* data, size_t sizeInBytes, GLenum usage = GL_STATIC_DRAW);

	/**
	 * @brief Updates a sub-range of the buffer.
	 * @tparam T Element type of the data.
	 * @param data Source data to upload.
	 * @param offset Byte offset into the buffer.
	 * 
	 * @warning The update must not exceed the current size of the buffer.
	 * If it does the operation is aborted.
	 */
	template <typename T>
	void updateData(const std::vector<T>& data, size_t offset = 0) {
		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER,
					"Cannot update an uninitialized VBO"
				);
			#endif

			return;
		}

		size_t dataSize = data.size() * sizeof(T);
		if (offset + dataSize > size) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER,
					"VBO %u: Update would overflow buffer (offset %zu + data %zu > buffer %zu)",
					id, offset, dataSize, size
				);
			#endif

			return;
		}

		bind();
		glBufferSubData(GL_ARRAY_BUFFER, offset, dataSize, data.data());
	}

	/**
	 * @brief Returns the OpenGL buffer handle.
	 */
	GLuint getID() const { return id; }

	/**
	 * @brief Returns the size of the buffer in bytes.
	 */
	size_t getSize() const { return size; }

	/**
	 * @brief Checks whether the buffer has been created.
	 * @return True if the OpenGL buffer exists.
	 */
	bool isValid() const { return id != 0; }
};

} // namespace Blackthorn::Graphics