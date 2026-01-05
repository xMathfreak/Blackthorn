#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <vector>

namespace Blackthorn::Graphics {

/**
 * @brief Describes a single vertex attribute layout entry.
 * 
 * Used to define how vertex data is interpreted by the GPU
 * when bound to a Vertex Array Object.
 */
struct VertexAttribute {
	/// Attribute index/location in the shader
	GLuint index;

	/// Number of components (e.g. 3 for vec3)
	GLint size;

	/// Component type (e.g. GL_FLOAT)
	GLenum type;

	/// Byte stride between consecutive vertices
	GLsizei stride;

	/// Byte offset from the start of the vertex
	size_t offset;

	/// Whether fixed-point data should be normalized
	bool normalized;
};

/**
 * @brief RAII Wrapper for an OpenGL Vertex Array Object.
 * 
 * Copying is disallowed to enforce unique ownership of the OpenGL
 * resource. Move semantics are supported.
 * 
 * @note Requires a valid OpenGL context to be current on the calling thread.
 * @note Attribute configuration assumes the appropriate VBO is bound to GL_ARRAY_BUFFER at the time of setup.
 */
class BLACKTHORN_API VAO {
private:
	/**
	 * @brief Tracks the currently bound VAO.
	 * 
	 * Used to avoid redundant bind() calls and allow isBound() checks.
	 */
	static inline GLuint currentVAO = 0;

	/// OpenGL VAO handle (0 if uninitialized)
	GLuint id = 0;

public:
	/**
	 * @brief Constructs an empty VAO without creating the OpenGL object.
	 */
	VAO() = default;

	/**
	 * @brief Constructs a VAO and optionally creates the OpenGL object.
	 * @param createNow If true, create() is called immediately.
	 */
	explicit VAO(bool createNow);

	/**
	 * @brief Destroys the VAO and releases the OpenGL object.
	 * 
	 * Safe to call even if the VAO was never created.
	 */
	~VAO();

	/// Copy construction is disabled (unique ownership)
	VAO(const VAO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	VAO& operator=(const VAO&) = delete;

	/**
	 * @brief Move-constructs a VAO, transferring ownership.
	 * @param other VAO to move from.
	 */
	VAO(VAO&& other) noexcept;

	/**
	 * @brief Move-assigns a VAO, transferring ownership.
	 * @param other VAO to move from.
	 * @return Reference to this object.
	 */
	VAO& operator=(VAO&& other) noexcept;

	/**
	 * @brief Creates the OpenGL VAO
	 * 
	 * If the VAO already exists, this function has no effect.
	 */
	void create();

	/**
	 * @brief Destroys the OpenGL VAO.
	 * 
	 * After calling this, isValid() will return false.
	 */
	void destroy();

	/**
	 * @brief Binds this VAO.
	 */
	void bind() const;

	/**
	 * @brief Unbinds any VAO.
	 */
	static void unbind();

	/**
	 * @brief Enables and defines a vertex attribute.
	 * 
	 * @param index Attribute index/location.
	 * @param size Number of components per vertex.
	 * @param type Component data type.
	 * @param stride Byte stride offset between vertices.
	 * @param offset Byte offset of the attribute.
	 * @param normalized Whether fixed-point values are normalized.
	 * 
	 * @pre A VBO must be bound to GL_ARRAY_BUFFER.
	 * @pre This VAO must be bound.
	 */
	void enableAttrib(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset, bool normalized = false);

	/**
	 * @brief Disables a vertex attribute.
	 * @param index Attribute index/location.
	 * 
	 * @pre This VAO must be bound.
	 */
	void disableAttrib(GLuint index);

	/**
	 * @brief Configures multiple vertex attributes in sequence.
	 * @param attributes List of vertex attribute descriptions.
	 * 
	 * @pre A VBO must be bound to GL_ARRAY_BUFFER.
	 * @pre This VAO must be bound.
	 */
	void setLayout(const std::vector<VertexAttribute>& attributes) {
		for (const auto& attr : attributes)
			enableAttrib(attr.index, attr.size, attr.type, attr.stride, attr.offset, attr.normalized);
	}

	/**
	 * @brief Returns the OpenGL VAO handle.
	 */
	GLuint getID() const noexcept { return id; }

	/**
	 * @brief Checks whether this VAO is currently bound.
	 */
	bool isBound() const noexcept { return currentVAO == id; }

	/**
	 * @brief Checks whether the VAO has been created.
	 */
	bool isValid() const noexcept { return id != 0; }

	/**
	 * @brief releases ownership of the VAO handle.
	 * @return The raw OpenGL VAO handle.
	 */
	GLuint takeHandle() noexcept;
};

} // namespace Blackthorn::Graphics