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
		BT_WARN(std::format("VBO already created (ID: {})", id));
		return;
	}

	glGenBuffers(1, &id);

	BT_DEBUG(std::format("VBO created (ID: {})", id));

}

void VBO::bind() const {
	if (id == 0) {
		BT_WARN("Attempting to bind unitialized VBO. Request ignored.");
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
		BT_WARN("Attempting to set data of uninitialized vertex buffer. Creating buffer automatically");
		create();
	}

	bind();
	glBufferData(GL_ARRAY_BUFFER, sizeInBytes, data, usage);
	size = sizeInBytes;

	BT_DEBUG(std::format("VBO {}: Uploaded {} bytes", id, size));
}

} // namespace Blackthorn::Graphics