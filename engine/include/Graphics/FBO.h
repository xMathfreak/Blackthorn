#pragma once

#include "Core/Export.h"
#include <Graphics/Texture.h>

#include <glad/glad.h>

#include <memory>

namespace Blackthorn::Graphics {

class BLACKTHORN_API FBO {
private:
	GLuint id = 0;
	GLsizei width;
	GLsizei height;
	std::unique_ptr<Texture> colorAttachment;

public:
	FBO(GLsizei w, GLsizei h);
	~FBO();

	FBO(const FBO&) = delete;
	FBO& operator=(const FBO&) = delete;

	FBO(FBO&& other) noexcept;
	FBO& operator=(FBO&& other) noexcept;

	void bind() const;
	static void unbind();

	void destroy();

	const Texture& getTexture() const;
};

} // namespace Blackthorn::Graphics
