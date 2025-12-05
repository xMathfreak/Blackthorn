#include "Graphics/Texture.h"

#include <SDL3_image/SDL_image.h>

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

namespace Blackthorn::Graphics {

GLenum Texture::toGLFilter(TextureFilter filter) {
	switch (filter) {
		case TextureFilter::Nearest:
			return GL_NEAREST;
		case TextureFilter::Linear:
			return GL_LINEAR;
		default:
			return GL_LINEAR;
	}
}

GLenum Texture::toGLWrap(TextureWrap wrap) {
	switch (wrap) {
		case TextureWrap::Repeat:
			return GL_REPEAT;
		case TextureWrap::MirroredRepeat:
			return GL_MIRRORED_REPEAT;
		case TextureWrap::ClampToEdge:
			return GL_CLAMP_TO_EDGE;
		case TextureWrap::ClampToBorder:
			return GL_CLAMP_TO_BORDER;
		default:
			return GL_REPEAT;
	}
}

void Texture::applyParams() {
	if (id == 0)
		return;

	bind();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params.generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : toGLFilter(params.minFilter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toGLFilter(params.magFilter));

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap(params.wrapS));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap(params.wrapT));

}

Texture::Texture(const std::string& path, const TextureParams& parameters) {
	loadFromFile(path, parameters);
}

Texture::Texture(int w, int h, int ch, const void* data, const TextureParams& parameters) {
	loadFromMemory(w, h, ch, data, parameters);
}

Texture::Texture(int w, int h, int ch, const TextureParams& parameters) {
	create(w, h, ch, parameters);
}

Texture::~Texture() {
	destroy();
}

Texture::Texture(Texture&& other) noexcept
	: id(other.id)
	, width(other.width)
	, height(other.height)
	, channels(other.channels)
	, params(other.params)
{
	other.id = 0;
	other.width = 0;
	other.height = 0;
	other.channels = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		width = other.width;
		height = other.height;
		channels = other.channels;
		params = other.params;

		other.id = 0;
		other.width = 0;
		other.height = 0;
		other.channels = 0;
	}

	return *this;
}

bool Texture::loadFromFile(const std::string& path, const TextureParams& parameters) {
	this->params = parameters;

	SDL_Surface* surface = IMG_Load(path.c_str());
	if (!surface) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(
				SDL_LOG_CATEGORY_RENDER,
				"Failed to load texture '%s': '%s",
				path.c_str(), SDL_GetError()
			);
		#endif

		return false;
	}

	GLenum format = GL_RGBA;
	GLenum internalFormat = GL_RGBA8;

	switch (surface->format) {
		case SDL_PIXELFORMAT_RGB24:
			format = GL_RGB;
			internalFormat = GL_RGB8;
			channels = 3;
			break;
		case SDL_PIXELFORMAT_RGBA32:
			format = GL_RGBA;
			internalFormat = GL_RGBA8;
			channels = 4;
			break;
		default:
			SDL_Surface* convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
			SDL_DestroySurface(surface);

			if (!convertedSurface)
				return false;

			surface = convertedSurface;
			format = GL_RGBA;
			internalFormat = GL_RGBA8;
			channels = 4;
			break;
	}

	width = surface->w;
	height = surface->h;

	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

	if (params.generateMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

	applyParams();
	SDL_DestroySurface(surface);

	return true;
}

bool Texture::loadFromMemory(int w, int h, int ch, const void* data, const TextureParams& parameters) {
	if (data == nullptr || w <= 0 || h <= 0 || ch < 1 || ch > 4) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Invalid texture parameters");
		#endif

		return false;
	}

	params = parameters;
	width = w;
	height = h;
	channels = ch;

	GLenum format;
	GLenum internalFormat;

	switch (channels) {
		case 1:
			format = GL_RED;
			internalFormat = GL_R8;
			break;
		case 2:
			format = GL_RG;
			internalFormat = GL_RG8;
			break;
		case 3:
			format = GL_RGB;
			internalFormat = GL_RGB8;
			break;
		case 4:
			format = GL_RGBA;
			internalFormat = GL_RGBA8;
			break;
		default:
			return false;
	}

	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

	if (params.generateMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

	applyParams();

	return true;
}

bool Texture::create(int w, int h, int ch, const TextureParams& parameters) {
	if (w <= 0 || h <= 0 || ch < 1 || ch > 4) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Invalid texture dimensions");
		#endif

		return false;
	}

	this->params = parameters;
	width = w;
	height = h;
	channels = ch;

	GLenum format;
	GLenum internalFormat;

	switch (channels) {
		case 1:
			format = GL_RED;
			internalFormat = GL_R8;
			break;
		case 2:
			format = GL_RG;
			internalFormat = GL_RG8;
			break;
		case 3:
			format = GL_RGB;
			internalFormat = GL_RGB8;
			break;
		case 4:
			format = GL_RGBA;
			internalFormat = GL_RGBA8;
			break;
		default:
			return false;
	}

	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

	if (params.generateMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

	applyParams();

	return true;
}

void Texture::destroy() {
	if (id != 0) {
		glDeleteTextures(1, &id);
		id = 0;
		width = 0;
		height = 0;
		channels = 0;
	}
}

void Texture::bind(GLuint slot) const {
	if (id == 0) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Attempting to bind invalid texture");
		#endif

		return;
	}

	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::unbind(GLuint slot) {
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::updateRegion(int x, int y, int w, int h, const void* data) {
	if (id == 0 || data == nullptr) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cannot update invalid texture");
		#endif

		return;
	}

	if (x < 0 || y < 0 || x + w > width || y + h > height) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogError(
				SDL_LOG_CATEGORY_RENDER, 
				"Texture update region out of bounds (%d, %d, %d, %d) for %d x %d texture",
				x, y, w, h, width, height
			);
		#endif

		return;
	}

	GLenum format;
	switch (channels) {
		case 1:
			format = GL_RED;
			break;
		case 2:
			format = GL_RG;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			return;
	}

	bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, format, GL_UNSIGNED_BYTE, data);
	if (params.generateMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);
}

Texture Texture::createDefault() {
	unsigned char pixel[] = { 255, 255, 255, 255 };
	TextureParams params;

	return Texture(1, 1, 4, pixel, params);
}

} // namespace Blackthorn::Graphics