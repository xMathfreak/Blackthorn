#include "Graphics/Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

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

		std::string errorMsg = std::string(shaderTypeToString(type)) + " shader compilation failed:\n" + log;

		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s", errorMsg.c_str());
		#endif

		throw std::runtime_error(errorMsg);
	}

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("%s shader compiled successfully.", shaderTypeToString(type));
	#endif

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

		std::string errorMsg = "Shader program linking failed:\n" + log;

		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s", errorMsg.c_str());
		#endif

		throw std::runtime_error(errorMsg);
	}

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Shader program linked successfully (ID: %u)", programID);
	#endif

		glDetachShader(programID, vertexShader);
		glDetachShader(programID, fragmentShader);
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Loading shader: %s, %s", vertexPath.c_str(), fragmentPath.c_str());
	#endif	

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
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(
				SDL_LOG_CATEGORY_RENDER,
				"Attempting to bind invalid shader"
			);
		#endif

		return;
	}

	glUseProgram(programID);
}

void Shader::unbind() {
	glUseProgram(0);
}

GLuint Shader::getUniformLocation(const std::string& name) {
	if (auto it = uniformCache.find(name); it != uniformCache.end())
		return it->second;

	GLint location = glGetUniformLocation(programID, name.c_str());
	
	#ifdef BLACKTHORN_DEBUG
		if (location == -1)
			SDL_LogWarn(
				SDL_LOG_CATEGORY_RENDER,
				"Uniform '%s' not found in shader program %u", 
				name.c_str(), programID
			);
	#endif

	uniformCache[name] = location;
	return location;
}

void Shader::setInt(const std::string& name, int value) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniform1i(location, value);
}

void Shader::setFloat(const std::string& name, float value) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniform1f(location, value);
}

void Shader::setVec2(const std::string& name, float x, float y) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniform2f(location, x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniform3f(location, x, y, z);
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniform4f(location, x, y, z, w);
}

void Shader::setMat4(const std::string& name, const float* value) {
	GLuint location = getUniformLocation(name);
	if (location != -1u)
		glUniformMatrix4fv(location, 1, GL_FALSE, value);
}

} // namespace Blackthorn::Graphics