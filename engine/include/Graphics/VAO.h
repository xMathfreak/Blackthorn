#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <vector>

namespace Blackthorn::Graphics {

struct VertexAttribute {
	GLuint index;
	GLint size;
	GLenum type;
	GLsizei stride;
	size_t offset;
	bool normalized;
};

class BLACKTHORN_API VAO {
private:
	static inline GLuint currentVAO = 0;
	GLuint id = 0;

public:
	VAO() = default;
	explicit VAO(bool createNow);
	~VAO();

	VAO(const VAO&) = delete;
	VAO& operator=(const VAO&) = delete;

	VAO(VAO&& other) noexcept;
	VAO& operator=(VAO&& other) noexcept;

	void create();
	void destroy();

	void bind() const;
	static void unbind();

	void enableAttrib(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset, bool normalized = false);
	void disableAttrib(GLuint index);

	void setLayout(const std::vector<VertexAttribute>& attributes) {
		for (const auto& attr : attributes)
			enableAttrib(attr.index, attr.size, attr.type, attr.stride, attr.offset, attr.normalized);
	}

	GLuint getID() const noexcept { return id; }
	bool isBound() const noexcept { return currentVAO == id; }
	bool isValid() const noexcept { return id != 0; }

	GLuint takeHandle() noexcept;
};

} // namespace Blackthorn::Graphics