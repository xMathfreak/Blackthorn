#pragma once

#include "Core/Export.h"

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <glad/glad.h>

#include <utility>

namespace Blackthorn::Graphics {

/**
 * @brief RAII Wrapper for an OpenGL Uniform Buffer Object.
 * 
 * Manages the lifetime and data synchronization of a uniform buffer storing
 * a trivially copyable C++ struct.
 * 
 * The template parameter @p T represents the CPU-side layout of the buffer.
 * Te layout of @p T must match the GLSL uniform block layout exactly
 * (typically std140 or std430).
 * 
 * Copying is disallowed to enforce unique ownership of the OpenGL resource.
 * Move semantics are supported.
 * 
 * @tparam T Struct type for representing the uniform block.
 * 
 * @note Requires a valid OpenGL context to be current on the calling thread.
 * @warning No validation is performed to ensure that T matches the GLSL layout.
 */
template <typename T>
class BLACKTHORN_API UBO {
private:
	/// OpenGL buffer object handle
	GLuint id = 0;

	/// CPU-side copy of the uniform data
	T data;

public:
	/**
	 * @brief Creates a uniform buffer and allocates storage.
	 * @param usage OpenGL usage hint (e.g. GL_DYNAMIC_DRAW).
	 * 
	 * Allocates buffer storage of size `sizeof(T)` with no initial data.
	 */
	UBO(GLenum usage = GL_DYNAMIC_DRAW) {
		glGenBuffers(1, &id);
		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), nullptr, usage);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		#ifdef BLACKTHORN_DEBUG
			SDL_Log("UBO created (ID: %u, Size: %lld)", id, sizeof(T));
		#endif
	}

	/**
	 * @brief Destroys the UBO and releases the OpenGL buffer.
	 */
	~UBO() {
		destroy();
	}

	/// Copy construction is disabled (unique ownership)
	UBO(const UBO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	UBO& operator=(const UBO&) = delete;

	/**
	 * @brief Move-constructs a UBO, transferring ownership.
	 * @param other UBO to move from.
	 */
	UBO(UBO&& other) noexcept
		: id(other.id)
		, data(std::move(other.data))
	{
		other.id = 0;
	}

	/**
	 * @brief Move-assigns a UBO, transferring ownership.
	 * @param other UBO to move from.
	 * @return Reference to this object.
	 */
	UBO& operator=(UBO&& other) noexcept {
		if (this != &other) {
			destroy();

			id = other.id;
			data = other.data;

			other.id = 0;
		}

		return *this;
	}

	/**
	 * @brief Destroys the OpenGL buffer.
	 * 
	 * Safe to call multiple times.
	 */
	void destroy() {
		if (id != 0) {
			glDeleteBuffers(1, &id);
			id = 0;
		}
	}

	/**
	 * @brief Binds the UBO to a uniform binding point.
	 * @param bindingPoint Uniform buffer binding point.
	 * 
	 * The shader must reference the same binding point for access.
	 */
	void bind(GLuint bindingPoint) const {
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, id);
	}

	/**
	 * @brief Updates the entire uniform buffer.
	 * @param newData New uniform data.
	 * 
	 * Copies the data to CPU-side cache and uploads it to the GPU.
	 */
	void setData(const T& newData) {
		data = newData;
		upload();
	}

	/**
	 * @brief Uploads the entire CPU-side data to the GPU buffer.
	 */
	void upload() const {
		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	/**
	 * @brief Uploads a single field from the uniform struct.
	 * @tparam Field Type of the field.
	 * @param fieldPtr Pointer-to-member identifying the field.
	 * 
	 * Computes the byte offset of the field within T and updates only that 
	 * sub-range of the buffer
	 * 
	 * @warning This relies on the standard layout behavior and assumes that
	 * the CPU struct layout matches the GLSL uniform block layout.
	 */
	template <typename Field>
	void uploadField(Field T::* fieldPtr) {
		size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T const*>(0)->*fieldPtr));
		const Field& fieldValue = data.*fieldPtr;

		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(Field), &fieldValue);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	/**
	 * @brief Returns a mutable reference to the CPU-side data.
	 */
	T& getData() { return data; }

	/**
	 * @brief Returns a const reference to the CPU-side data.
	 */
	const T& getData() const { return data; }
};

} // namespace Blackthorn::Graphics
