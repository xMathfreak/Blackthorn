#pragma once

#include "Core/Export.h"

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <glad/glad.h>

#include <utility>

namespace Blackthorn::Graphics {

template <typename T>
class BLACKTHORN_API UBO {
private:
	GLuint id = 0;
	T data;

public:
	UBO(GLenum usage = GL_DYNAMIC_DRAW) {
		glGenBuffers(1, &id);
		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), nullptr, usage);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		#ifdef BLACKTHORN_DEBUG
			SDL_Log("UBO created (ID: %u, Size: %lld)", id, sizeof(T));
		#endif
	}

	~UBO() {
		destroy();
	}

	UBO(const UBO&) = delete;
	UBO& operator=(const UBO&) = delete;

	UBO(UBO&& other) noexcept
		: id(other.id)
		, data(std::move(other.data))
	{
		other.id = 0;
	}

	UBO& operator=(UBO&& other) noexcept {
		if (this != &other) {
			destroy();

			id = other.id;
			data = other.data;

			other.id = 0;
		}

		return *this;
	}

	void destroy() {
		if (id != 0) {
			glDeleteBuffers(1, &id);
			id = 0;
		}
	}

	void bind(GLuint bindingPoint) const {
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, id);
	}

	void setData(const T& newData) {
		data = newData;
		upload();
	}

	void upload() const {
		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	template <typename Field>
	void uploadField(Field T::* fieldPtr) {
		size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T const*>(0)->*fieldPtr));
		const Field& fieldValue = data.*fieldPtr;

		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(Field), &fieldValue);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	T& getData() { return data; }
	const T& getData() const { return data; }
};

} // namespace Blackthorn::Graphics
