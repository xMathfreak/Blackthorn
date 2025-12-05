#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

#include <vector>

namespace Blackthorn::Graphics {

class BLACKTHORN_API EBO {
private:
	GLuint id = 0;
	size_t count = 0;
	size_t size = 0;
	GLenum indexType = 0;

public:
	EBO() = default;
	explicit EBO(bool createNow);
	~EBO();

	EBO(const EBO&) = delete;
	EBO& operator=(const EBO&) = delete;

	EBO(EBO&& other) noexcept;
	EBO& operator=(EBO&& other) noexcept;

	void create();
	void destroy();

	void bind() const;
	static void unbind();

	void setData(const void* data, size_t indexCount, GLenum type, GLenum usage = GL_STATIC_DRAW);

	template <typename T>
	void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
		static_assert(
			std::is_same_v<T, GLuint> || 
			std::is_same_v<T, GLushort> || 
			std::is_same_v<T, GLubyte>,
			"Index type must be GLuint, GLushort, or GLubyte"
		);

		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to set data on uninitialized EBO");
			#endif
			create();
		}

		bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), usage);
		
		count = data.size();
		size = data.size() * sizeof(T);
		
		if constexpr (std::is_same_v<T, GLuint>) {
			indexType = GL_UNSIGNED_INT;
		} else if constexpr (std::is_same_v<T, GLushort>) {
			indexType = GL_UNSIGNED_SHORT;
		} else if constexpr (std::is_same_v<T, GLubyte>) {
			indexType = GL_UNSIGNED_BYTE;
		}

		#ifdef BLACKTHORN_DEBUG
			const char* typeStr = (indexType == GL_UNSIGNED_INT) ? 
				"GLuint" :
				(indexType == GL_UNSIGNED_SHORT) ? "GLushort" : "GLubyte";
			SDL_Log("EBO %u: Uploaded %lld indices (%s, %lld bytes)", 
					id, count, typeStr, size);
		#endif
	}

	template <typename T>
	void updateData(const std::vector<T>& data, size_t offsetInBytes = 0) {
		static_assert(
			std::is_same_v<T, GLuint> || 
			std::is_same_v<T, GLushort> || 
			std::is_same_v<T, GLubyte>,
			"Index type must be GLuint, GLushort, or GLubyte"
		);

		if (id == 0) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cannot update uninitialized EBO");
			#endif
			
			return;
		}

		size_t dataSize = data.size() * sizeof(T);
		if (offsetInBytes + dataSize > size) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogError(
					SDL_LOG_CATEGORY_RENDER, 
					"EBO %u: Update would overflow buffer (offset %zu + data %zu > buffer %zu)", 
					id, offsetInBytes, dataSize, size
				);
			#endif

			return;
		}

		bind();
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offsetInBytes, dataSize, data.data());
	}

	bool isValid() const noexcept { return id != 0; }
	GLuint getID() const noexcept { return id; }
	size_t getCount() const noexcept { return count; }
	size_t getSize() const noexcept { return size; }
	GLenum getIndexType() const noexcept { return indexType; }
};

} // namespace Blackthorn::Graphics