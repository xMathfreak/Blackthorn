#include "Graphics/EBO.h"

namespace Blackthorn::Graphics {

EBO::EBO(bool createNow) {
	if (createNow)
		create();
}

EBO::~EBO() {
	destroy();
}

EBO::EBO(EBO&& other) noexcept
	: id(other.id)
	, count(other.count)
	, size(other.size)
	, indexType(other.indexType)
{
	other.id = 0;
	other.count = 0;
	other.size = 0;
	other.indexType = 0;
}

EBO& EBO::operator=(EBO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		count = other.count;
		size = other.size;
		indexType = other.indexType;

		other.id = 0;
		other.count = 0;
		other.size = 0;
		other.indexType = 0;
	}

	return *this;
}

void EBO::create() {
	if (id != 0) {	
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "EBO already created (ID: %u)", id);
		#endif
			
		return;
	}

	glGenBuffers(1, &id);

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("EBO created (ID: %u)", id);
	#endif
}

void EBO::destroy() {
	if (id != 0) {
		glDeleteBuffers(1, &id);
		id = 0;
		count = 0;
		size = 0;
		indexType = 0;
	}
}

void EBO::bind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void EBO::unbind() {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::setData(const void* data, size_t indexCount, GLenum type, GLenum usage) {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to set data on uninitialized EBO");
		#endif
		
		create();
	}

	size_t elementSize = 0;
	switch (type) {
		case GL_UNSIGNED_INT:
			elementSize = sizeof(GLuint);
			break;
		case GL_UNSIGNED_SHORT:
			elementSize = sizeof(GLushort);
			break;
		case GL_UNSIGNED_BYTE:
			elementSize = sizeof(GLubyte);
			break;
		default:
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Invalid index type: 0x%x", type);
			#endif
			
			return;
	}

	bind();
	size_t sizeInBytes = indexCount * elementSize;
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeInBytes, data, usage);

	count = indexCount;
	size = sizeInBytes;
	indexType = type;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("EBO %u: Uploaded %lld indices (type 0x%x, %lld bytes)", id, count, indexType, size);
	#endif
}

} // namespace Blackthorn::Graphics