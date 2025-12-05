#pragma once

#include "Core/Export.h"

#include <glad/glad.h>

#include <string>

namespace Blackthorn::Graphics {

enum class TextureFilter {
	Nearest,
	Linear
};

enum class TextureWrap {
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	ClampToBorder
};

struct TextureParams {
	TextureFilter minFilter = TextureFilter::Nearest;
	TextureFilter magFilter = TextureFilter::Nearest;
	TextureWrap wrapS = TextureWrap::Repeat;
	TextureWrap wrapT = TextureWrap::Repeat;
	bool generateMipmaps = false;
};

class BLACKTHORN_API Texture {
private:
	GLuint id = 0;
	int width = 0;
	int height = 0;
	int channels = 0;
	TextureParams params;

	void applyParams();

	static GLenum toGLFilter(TextureFilter filter);
	static GLenum toGLWrap(TextureWrap wrap);

public:
	Texture() = default;
	explicit Texture(const std::string& path, const TextureParams& parameters = TextureParams());

	Texture(int width, int height, int channels, const void* data, const TextureParams& parameters = TextureParams());
	Texture(int width, int height, int channels = 4, const TextureParams& parameters = TextureParams());

	~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	bool loadFromFile(const std::string& path, const TextureParams& parameters = TextureParams());
	bool loadFromMemory(int w, int h, int ch, const void* data, const TextureParams& parameters = TextureParams());

	bool create(int width, int height, int channels = 4, const TextureParams& parameters = TextureParams());
	void destroy();

	void bind(GLuint slot = 0) const;
	static void unbind(GLuint slot = 0);

	void updateRegion(int x, int y, int width, int height, const void* data);

	bool isValid() const noexcept { return id != 0; }
	GLuint getID() const noexcept { return id; }
	int getWidth() const noexcept { return width; }
	int getHeight() const noexcept { return height; }
	int getChannels() const noexcept { return channels; }
	const TextureParams& getParams() const noexcept { return params; }

	static Texture createDefault();
};

} // namespace Blackthorn::Graphics