#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <glad/gl.h>

#include "Core/Export.h"
#include "Core/Types/StringHash.h"

namespace Blackthorn::Graphics {

/**
 * @brief RAII wrapper for an OpenGL shader program.
 *
 * This class owns the linked program object and automatically deletes
 * it on destruction. Uniform locations are cached after first lookup
 * to reduce repeated OpenGL calls.
 *
 * Copying is disallowed to enforce unique ownership of the OpenGL program.
 * Move semantics are supported.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API Shader {
private:
	/// OpenGL program object handle (0 if uninitialized)
	GLuint programID = 0;

	/// Cache of uniform locations indexed by Uniform name.
	std::unordered_map<std::string, GLint, StringHash, std::equal_to<>> uniformCache;

	/**
	 * @brief Links a shader program from compiled shaders.
	 * @param vertexShader Compiled vertex shader handle.
	 * @param fragmentShader Compiled fragment shader handle.
	 *
	 * Takes ownership of the linked program but not of the individual shader objects.
	 */
	void linkProgram(GLuint vertexShader, GLuint fragmentShader);

	/**
	 * @brief Compiles a shader from source code.
	 * @param source GLSL source code.
	 * @param type Shader type (e.g. GL_VERTEX_SHADER).
	 * @return OpenGL shader object handle.
	 */
	GLuint compileShader(const std::string& source, GLenum type);

	/**
	 * @brief Retrieves and caches a uniform location.
	 * @param name Uniform name..
	 * @return Uniform location, or -1 if not.
	 */
	GLint getUniformLocation(std::string_view name);

public:
	/**
	 * @brief Constructs an empty shader program.
	 */
	Shader() = default;

	/**
	 * @brief Creates and links a shader program from source files.
	 * @param vertexPath Path to the vertex shader source code.
	 * @param fragmentPath Path to the fragment shader source code.
	 *
	 * Compiles the shaders, links the program and deletes the intermediate shader objects.
	 */
	Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);

	/**
	 * @brief Destroys the shader program and releases OpenGL resources.
	 */
	~Shader();

	/// Copy construction is disabled (unique ownership)
	Shader(const Shader&) = delete;
	/// Copy assignment is disabled (unique ownership)
	Shader& operator=(const Shader&) = delete;

	/**
	 * @brief Move constructs a shader program, transferring ownership.
	 * @param other Shader to move from.
	 */
	Shader(Shader&& other) noexcept;
	/**
	 * @brief Move assigns a shader program, transferring ownership.
	 * @param other Shader to move from.
	 * @return Reference to this object.
	 */
	Shader& operator=(Shader&& other) noexcept;

	/**
	 * @brief Binds this shader program for use.
	 */
	void bind() const;

	/**
	 * @brief Unbinds any currently bound shader program.
	 */
	static void unbind();

	/**
	 * @brief Destroys the shader program.
	 */
	void destroy();

	/**
	 * @brief Checks whether the shader program is valid.
	 */
	bool isValid() const noexcept { return programID != 0; }

	/**
	 * @brief Returns the OpenGL program handle.
	 */
	GLuint id() const noexcept { return programID; }

	/**
	 * @brief Sets an boolean uniform.
	 * @param name Uniform name.
	 * @param value Boolean value.
	 */
	void setBool(std::string_view name, bool value);

	/**
	 * @brief Sets an integer uniform.
	 * @param name Uniform name.
	 * @param value Integer value.
	 */
	void setInt(std::string_view name, int value);

	/**
	 * @brief Sets a float uniform
	 * @param name Uniform name.
	 * @param value Float value.
	 */
	void setFloat(std::string_view name, float value);

	/**
	 * @brief Sets a vec2 uniform
	 * @param name Uniform name.
	 * @param x X component.
	 * @param y Y component.
	 */
	void setVec2(std::string_view name, float x, float y);

	/**
	 * @brief Sets a vec3 uniform
	 * @param name Uniform name.
	 * @param x X component.
	 * @param y Y component.
	 * @param z Z component.
	 */
	void setVec3(std::string_view name, float x, float y, float z);

	/**
	 * @brief Sets a vec4 uniform
	 * @param name Uniform name.
	 * @param x X component.
	 * @param y Y component.
	 * @param z Z component.
	 * @param w W component.
	 */
	void setVec4(std::string_view name, float x, float y, float z, float w);

	/**
	 * @brief Sets a 4x4 matrix uniform.
	 * @param name Uniform name.
	 * @param value Pointer to 16 consecutive floats.
	 *
	 * The matrix is uploaded as-is; column/row major expectations must
	 * match the shader definition.
	 */
	void setMat4(std::string_view name, const float* value);
};

} // namespace Blackthorn::Graphics