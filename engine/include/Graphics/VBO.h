#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <vector>

namespace Blackthorn::Graphics {

class BLACKTHORN_API VBO {
private:
	GLuint id = 0;
	size_t size = 0;

public:
	VBO() = default;
	explicit VBO(bool createNow);
	~VBO();

	VBO(const VBO&) = delete;
	VBO& operator=(const VBO&) = delete;

	VBO(VBO&& other) noexcept;
	VBO& operator=(VBO&& other) noexcept;

	void create();
	void destroy();

	void bind() const;
	static void unbind();

	template <typename T>
	void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
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
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), usage);
		size = data.size() * sizeof(T);

		#ifdef BLACKTHORN_DEBUG
			SDL_Log(
				"VBO %u: Uploaded %zu bytes (%zu elements of size %zu)",
				id, size, data.size(), sizeof(T)
			);
		#endif
	}

	void setData(const void* data, size_t sizeInBytes, GLenum usage = GL_STATIC_DRAW);

	template <typename T>
	void updateData(const std::vector<T>& data, size_t offset = 0) {
		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER,
					"Cannot update an uninitialized VBO"
				);
			#endif

			return;
		}

		size_t dataSize = data.size() * sizeof(T);
		if (offset + dataSize > size) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER,
					"VBO %u: Update would overflow buffer (offset %zu + data %zu > buffer %zu)",
					id, offset, dataSize, size
				);
			#endif

			return;
		}

		bind();
		glBufferSubData(GL_ARRAY_BUFFER, offset, dataSize, data.data());
	}

	GLuint getID() const { return id; }
	size_t getSize() const { return size; }
	bool isValid() const { return id != 0; }
};

} // namespace Blackthorn::Graphics