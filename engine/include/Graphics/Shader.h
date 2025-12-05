#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#include <string>
#include <unordered_map>

namespace Blackthorn::Graphics {

class BLACKTHORN_API Shader {
private:
	GLuint programID = 0;
	std::unordered_map<std::string, GLint> uniformCache;

	void linkProgram(GLuint vertexShader, GLuint fragmentShader);
	GLuint compileShader(const std::string& source, GLenum type);
	GLuint getUniformLocation(const std::string& name);

public:
	Shader() = default;
	Shader(const std::string& vertexPath, const std::string& fragmentPath);
	~Shader();

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	void bind() const;
	static void unbind();

	bool isValid() const noexcept { return programID != 0; }
	GLuint id() const noexcept { return programID; }

	void setInt(const std::string& name, int value);
	void setFloat(const std::string& name, float value);
	void setVec2(const std::string& name, float x, float y);
	void setVec3(const std::string& name, float x, float y, float z);
	void setVec4(const std::string& name, float x, float y, float z, float w);
	void setMat4(const std::string& name, const float* value);

};

} // namespace Blackthorn::Graphics