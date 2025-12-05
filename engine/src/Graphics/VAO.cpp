#include "Graphics/VAO.h"

namespace Blackthorn::Graphics {

VAO::VAO(bool createNow) {
	if (createNow)
		create();
}

VAO::~VAO() {
	destroy();
}

VAO::VAO(VAO&& other) noexcept
	: id(other.id)
{
	other.id = 0;
}

VAO& VAO::operator=(VAO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		other.id = 0;
	}

	return *this;
}


void VAO::create() {
	if (id != 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "VAO already created (ID: %u)", id);
		#endif

		return;
	}

	glGenVertexArrays(1, &id);
	#ifdef BLACKTHORN_DEBUG
		SDL_Log("VAO created (ID: %u)", id);
	#endif
}

void VAO::bind() const {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to bind uninitialized VAO");
		#endif

		return;
	}

	if (currentVAO != id) {
		glBindVertexArray(id);
		currentVAO = id;
	}
}

void VAO::unbind() {
	if (currentVAO != 0) {
		glBindVertexArray(0);
		currentVAO = 0;
	}
}

void VAO::enableAttrib(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset, bool normalized) {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cannot configure attributes on uninitialized VAO");
			return;
		#endif
	}

	if (!isBound()) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(
				SDL_LOG_CATEGORY_RENDER,
				"Configuring VAO %u attributes while not bound",
				id
			);
		#endif

		bind();
	}

	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, size, type, normalized ? GL_TRUE : GL_FALSE, stride, reinterpret_cast<const void*>(offset));

	#ifdef BLACKTHORN_DEBUG
		const char* typeStr = "unknown";

		switch (type) {
			case GL_FLOAT: 
				typeStr = "GL_FLOAT";
				break;
			case GL_INT:
				typeStr = "GL_INT";
				break;
			case GL_UNSIGNED_INT:
				typeStr = "GL_UNSIGNED_INT";
				break;
			case GL_BYTE:
				typeStr = "GL_BYTE";
				break;
			case GL_UNSIGNED_BYTE:
				typeStr = "GL_UNSIGNED_BYTE";
				break;
		}
		SDL_Log("VAO %u: Enabled attribute %u (size=%d, type=%s, stride=%d, offset=%lld, normalized=%u)",
		id, index, size, typeStr, stride, offset, normalized);
	#endif
}

void VAO::disableAttrib(GLuint index) {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER,
				"Cannot disable attributes on uninitialized VAO"
			);
		#endif

		return;
	}

	if (!isBound()) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
				"Disabling VAO %u attributes while not bound",
				id
			);
		#endif

		bind();
	}

	glDisableVertexAttribArray(index);

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("VAO %u: Disabled attribute %u", id, index);
	#endif
}

void VAO::destroy() {
	if (id != 0) {
		glDeleteVertexArrays(1, &id);

		if (currentVAO == id)
			currentVAO = 0;

		id = 0;
	}
}


GLuint VAO::takeHandle() noexcept {
	GLuint tmp = id;
	id = 0;

	if (currentVAO == tmp) {
		currentVAO = 0;
	}

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("VAO handle taken (ID: %u), ownership transferred", tmp);
	#endif

	return tmp;
}

} // namespace Blackthorn::Graphics