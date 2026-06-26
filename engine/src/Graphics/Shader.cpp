#include "Graphics/Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Debug/Logger.h"

namespace Blackthorn::Graphics {

static std::string readFile(const std::string& path) {
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("Failed to open file " + path);

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

static const char* shaderTypeToString(GLenum type) {
	switch (type) {
		case GL_VERTEX_SHADER:
			return "Vertex";
		case GL_FRAGMENT_SHADER:
			return "Fragment";
		case GL_GEOMETRY_SHADER:
			return "Geometry";
		default:
			return "Unknown";
	}
}

GLuint Shader::compileShader(const std::string& source, GLenum type) {
	GLuint shader = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success) {
		GLint logLength = 0;
		glad_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

		std::string log;
		if (logLength > 0) {
			log.resize(logLength);
			glGetShaderInfoLog(shader, logLength, nullptr, log.data());
		}

		glDeleteShader(shader);

		BT_ERROR("Shader: Compilation failed ({}):\n{}", shaderTypeToString(type), log);
		throw std::runtime_error("Shader compilation failed");
	}

	BT_DEBUG("Shader: Compiled ({})", shaderTypeToString(type));
	return shader;
}

void Shader::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
	programID = glCreateProgram();
	glAttachShader(programID, vertexShader);
	glAttachShader(programID, fragmentShader);
	glLinkProgram(programID);

	GLint success = 0;
	glGetProgramiv(programID, GL_LINK_STATUS, &success);

	if (!success) {
		GLint logLength = 0;
		glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);

		std::string log;
		if (logLength > 0) {
			log.resize(logLength);
			glad_glGetProgramInfoLog(programID, logLength, nullptr, log.data());
		}

		glDeleteProgram(programID);
		programID = 0;

		BT_ERROR("Shader: Linking failed:\n{}", log);
		throw std::runtime_error("Shader linking failed");
	}

	BT_DEBUG("Shader program linked successfully (ID: {})", programID);

	glDetachShader(programID, vertexShader);
	glDetachShader(programID, fragmentShader);
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
	BT_DEBUG("Shader: Loading (vertex: {}, fragment: {})", vertexPath, fragmentPath);

	try {
		std::string vertexSource = readFile(vertexPath);
		std::string fragmentSource = readFile(fragmentPath);

		GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
		GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

		linkProgram(vertexShader, fragmentShader);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	} catch(const std::exception& e) {
		if (programID != 0) {
			glDeleteProgram(programID);
			programID = 0;
		}

		throw;
	}
}

Shader::~Shader() {
	destroy();
}

void Shader::destroy() {
	if (programID != 0) {
		glDeleteProgram(programID);
		programID = 0;
	}
}

Shader::Shader(Shader&& other) noexcept
	: programID(other.programID)
	, uniformCache(std::move(other.uniformCache))
{
	other.programID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
	if (this != &other) {
		if (programID != 0)
			glDeleteProgram(programID);

		programID = other.programID;
		uniformCache = std::move(other.uniformCache);
		other.programID = 0;
	}

	return *this;
}

void Shader::bind() const {
	if (programID == 0) {
		BT_WARN("Shader: Attempting to bind invalid shader, request ignored");
		return;
	}

	glUseProgram(programID);
}

void Shader::unbind() {
	glUseProgram(0);
}

GLint Shader::getUniformLocation(std::string_view name) {
	if (auto it = uniformCache.find(name); it != uniformCache.end())
		return it->second;

	std::string nameStr{name};
	GLint location = glGetUniformLocation(programID, nameStr.c_str());

	if (location == -1)
		BT_WARN("Shader: Uniform '{}' not found in shader program {}", name, programID);

	uniformCache.emplace(std::move(nameStr), location);
	return location;
}

void Shader::setBool(std::string_view name, bool value) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform1i(location, value);
}

void Shader::setInt(std::string_view name, int value) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform1i(location, value);
}

void Shader::setFloat(std::string_view name, float value) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform1f(location, value);
}

void Shader::setVec2(std::string_view name, float x, float y) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform2f(location, x, y);
}

void Shader::setVec3(std::string_view name, float x, float y, float z) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform3f(location, x, y, z);
}

void Shader::setVec4(std::string_view name, float x, float y, float z, float w) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniform4f(location, x, y, z, w);
}

void Shader::setMat4(std::string_view name, const float* value) {
	GLint location = getUniformLocation(name);
	if (location != -1)
		glUniformMatrix4fv(location, 1, GL_FALSE, value);
}

} // namespace Blackthorn::Graphics