#include "Graphics/VAO.h"

#include "Debug/Logger.h"

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
		BT_WARN("VAO already exists (ID: {})", id);
		return;
	}

	glGenVertexArrays(1, &id);

	BT_DEBUG("VAO created (ID: {})", id);
}

void VAO::bind() const {
	if (id == 0) {
		BT_WARN("VAO: Attempting to bind uninitialized array object, request ignored.");
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
		BT_ERROR("VAO: Cannot configure attributes on uninitialized array object");
		return;
	}

	if (!isBound()) {
		BT_WARN("VAO: Attempting to configure attributes on unbound array object (ID: {}), binding now", id);
		bind();
	}

	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, size, type, normalized ? GL_TRUE : GL_FALSE, stride, reinterpret_cast<const void*>(offset));

	const char* typeStr = "Unknown";

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

	BT_DEBUG("VAO {}: Enabled attribute {} (size={}, type={}, stride={}, offset={}, normalized={})",
		id, index, size, typeStr, stride, offset, (normalized ? "true" : "false")
	);

}

void VAO::disableAttrib(GLuint index) {
	if (id == 0) {
		BT_ERROR("VAO: Attempting to disable attributes on uninitialized array object, request ignored");
		return;
	}

	if (!isBound()) {
		BT_WARN("VAO: Attempting to disable attributes on unbound array object (ID: {}), binding now", id);
		bind();
	}

	glDisableVertexAttribArray(index);
	BT_DEBUG("VAO {}: Disabled attribute {}", id, index);
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

	BT_DEBUG("VAO handle taken (ID: {}), ownership transferred", tmp);
	return tmp;
}

} // namespace Blackthorn::Graphics