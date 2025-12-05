#include "Graphics/VBO.h"

namespace Blackthorn::Graphics {

VBO::VBO(bool createNow) {
	if (createNow)
		create();
}

VBO::~VBO() {
	destroy();
}

VBO::VBO(VBO&& other) noexcept
	: id(other.id)
	, size(other.size)
{
	other.id = 0;
	other.size = 0;
}

VBO& VBO::operator=(VBO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		size = other.size;

		other.id = 0;
		other.size = 0;
	}

	return *this;
}

void VBO::create() {
	if (id != 0) {	
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "VBO already created (ID: %u)", id);
		#endif
			
		return;
	}

	glGenBuffers(1, &id);

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("VBO created (ID: %u)", id);
	#endif
}

void VBO::bind() const {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to bind unitialized VBO");
		#endif

		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, id);
}

void VBO::unbind() {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::destroy() {
	if (id != 0) {
		glDeleteBuffers(1, &id);
		id = 0;
	}
}

void VBO::setData(const void* data, size_t sizeInBytes, GLenum usage) {
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
	glBufferData(GL_ARRAY_BUFFER, sizeInBytes, data, usage);
	size = sizeInBytes;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log(
			"VBO %u: Uploaded %lld bytes",
			id, size
		);
	#endif
}

} // namespace Blackthorn::Graphics