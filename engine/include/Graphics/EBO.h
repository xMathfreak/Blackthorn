#pragma once

#include <vector>

#include <glad/glad.h>
#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include "Core/Export.h"

namespace Blackthorn::Graphics {

/**
 * @brief RAII wrapper for an OpenGL Element Buffer Object (index buffer).
 * 
 * Encapsulates creation, management and index data storage for indexed 
 * drawing using `glDrawElements()`.
 * 
 * The element buffer binding is stored in the currently bound VAO.
 * As a result, this buffer should be bound while the intended
 * VAO is active.
 * 
 * Copying is disabled to enforce unique ownership of the OpenGL resource.
 * Move semantics are supported.
 * 
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API EBO {
private:
	/// OpenGL buffer object handle (0 if uninitialized)
	GLuint id = 0;

	/// Number of indices stored in the buffer
	size_t count = 0;

	/// Size of the buffer
	size_t size = 0;

	/// OpenGL index type (GL_UNSIGNED_INT, GL_UNSIGNED_SHORT, GL_UNSIGNED_BYTE)
	GLenum indexType = 0;

public:
	/**
	 * @brief Constructs an empty EBO without creating the OpenGL buffer.
	 */
	EBO() = default;

	/**
	 * @brief Constructs an EBO and optionally creates the OpenGL buffer.
	 * @param createNow If true, create() is called immediately.
	 */
	explicit EBO(bool createNow);

	/**
	 * @brief Destroys the EBO and releases the OpenGL buffer.
	 * 
	 * Safe to call even if the buffer was never created.
	 */
	~EBO();

	/// Copy construction is disabled (unique ownership)
	EBO(const EBO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	EBO& operator=(const EBO&) = delete;

	/**
	 * @brief Move-constructs an EBO, transferring ownership.
	 * @param other EBO to move from
	 */
	EBO(EBO&& other) noexcept;

	/**
	 * @brief Move-assigns an EBO, transferring ownership
	 * @param other EBO to move from.
	 * @return Reference to this object.
	 */
	EBO& operator=(EBO&& other) noexcept;

	/**
	 * @brief Creates the OpenGL element buffer.
	 * 
	 * If the buffer already exists, this function has no effect.
	 */
	void create();

	/**
	 * @brief Destroys the OpenGL element buffer.
	 * 
	 * After calling this, isValid() will return false.
	 */
	void destroy();

	/**
	 * @brief Binds this EBO to GL_ELEMENT_ARRAY_BUFFER.
	 * 
	 * @note The binding is captured by the currently bound VAO.
	 */
	void bind() const;

	/**
	 * 	@brief Unbinds any EBO from GL_ELEMENT_ARRAY_BUFFER.
	 */
	static void unbind();

	/**
	 * @brief Allocates and uploads raw index data.
	 * @param data Pointer to index data.
	 * @param indexCount Number of indices.
	 * @param type Index type (GL_UNSIGNED_INT, GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE)
	 * @param usage OpenGL usage hint.
	 * 
	 * Replaces any existing buffer storage.
	 */
	void setData(const void* data, size_t indexCount, GLenum type, GLenum usage = GL_STATIC_DRAW);

	/**
	 * @brief Allocates and uploads index data from a vector.
	 * @tparam T Index element type.
	 * @param data Index data to upload.
	 * @param usage OpenGL usage hint.
	 * 
	 * The index type is inferred from T and stored for later draw calls.
	 * If the buffer has not been created yet, it will be created automatically.
	 */
	template <typename T>
	void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
		static_assert(
			std::is_same_v<T, GLuint> || 
			std::is_same_v<T, GLushort> || 
			std::is_same_v<T, GLubyte>,
			"Index type must be GLuint, GLushort, or GLubyte"
		);

		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to set data on uninitialized EBO");
			#endif
			create();
		}

		bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), usage);
		
		count = data.size();
		size = data.size() * sizeof(T);
		
		if constexpr (std::is_same_v<T, GLuint>) {
			indexType = GL_UNSIGNED_INT;
		} else if constexpr (std::is_same_v<T, GLushort>) {
			indexType = GL_UNSIGNED_SHORT;
		} else if constexpr (std::is_same_v<T, GLubyte>) {
			indexType = GL_UNSIGNED_BYTE;
		}

		#ifdef BLACKTHORN_DEBUG
			const char* typeStr = (indexType == GL_UNSIGNED_INT) ? 
				"GLuint" :
				(indexType == GL_UNSIGNED_SHORT) ? "GLushort" : "GLubyte";
			SDL_Log("EBO %u: Uploaded %lld indices (%s, %lld bytes)", 
					id, count, typeStr, size);
		#endif
	}

	/**
	 * @brief Updates a sub-range of the index buffer.
	 * @tparam T Index element type.
	 * @param data Index data to upload.
	 * @param offsetInBytes Byte offset into the buffer.
	 * 
	 * @warning The update must not exceed the current buffer size.
	 * If it does, the operation is aborted.
	 */
	template <typename T>
	void updateData(const std::vector<T>& data, size_t offsetInBytes = 0) {
		static_assert(
			std::is_same_v<T, GLuint> || 
			std::is_same_v<T, GLushort> || 
			std::is_same_v<T, GLubyte>,
			"Index type must be GLuint, GLushort, or GLubyte"
		);

		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cannot update uninitialized EBO");
			#endif
			
			return;
		}

		size_t dataSize = data.size() * sizeof(T);
		if (offsetInBytes + dataSize > size) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER, 
					"EBO %u: Update would overflow buffer (offset %zu + data %zu > buffer %zu)", 
					id, offsetInBytes, dataSize, size
				);
			#endif

			return;
		}

		bind();
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offsetInBytes, dataSize, data.data());
	}

	/**
	 * @brief Checks whether the buffer has been created.
	 */
	bool isValid() const noexcept { return id != 0; }

	/**
	 * @brief Returns the OpenGL buffer handle.
	 */
	GLuint getID() const noexcept { return id; }

	/**
	 * @brief Returns the number of indices stored in the buffer.
	 */
	size_t getCount() const noexcept { return count; }

	/**
	 * @brief Returns the size of the buffer in bytes
	 */
	size_t getSize() const noexcept { return size; }

	/**
	 * @brief Returns the OpenGL index type.
	 * 
	 * This value is intended to be passed directly to `glDrawElements()`
	 */
	GLenum getIndexType() const noexcept { return indexType; }
};

} // namespace Blackthorn::Graphics