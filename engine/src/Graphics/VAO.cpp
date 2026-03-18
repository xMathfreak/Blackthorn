#include "Graphics/VAO.h"

#include <format>

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
		BT_WARN(std::format("VAO already created (ID: {})", id));
		return;
	}

	glGenVertexArrays(1, &id);

	BT_DEBUG(std::format("VAO created (ID: {})", id));
}

void VAO::bind() const {
	if (id == 0) {
		BT_WARN("Attempting to bind uninitialized VAO");
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
		BT_ERROR("Cannot configure attributes on uninitialized VAO");
		return;
	}

	if (!isBound()) {
		BT_WARN(std::format("Attempting to configure attributes on VAO {} while not bound. Binding", id));
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

	BT_DEBUG(std::format("VAO {}: Enabled attribute {} (size={}, type={}, stride={}, offset={}, normalized={})",
		id, index, size, typeStr, stride, offset, (normalized ? "true" : "false")
	));

}

void VAO::disableAttrib(GLuint index) {
	if (id == 0) {
		BT_ERROR("Cannot disable attributes of an uninitialized VAO");
		return;
	}

	if (!isBound()) {
		BT_WARN(std::format("Attempting to disable attributes on VAO {} while not bound. Binding", id));
		bind();
	}

	glDisableVertexAttribArray(index);
	BT_DEBUG(std::format("VAO {}: Disabled attribute {}", id, index));
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

	BT_DEBUG(std::format("VAO handle taken (ID: {}), ownership transferred", tmp));
	return tmp;
}

} // namespace Blackthorn::Graphics