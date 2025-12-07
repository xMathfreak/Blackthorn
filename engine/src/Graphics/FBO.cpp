#include "Graphics/FBO.h"

namespace Blackthorn::Graphics {

FBO::FBO(GLsizei w, GLsizei h)
	: width(w)
	, height(h)
{
	glGenFramebuffers(1, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, id);

	colorAttachment = std::make_unique<Texture>(width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorAttachment->getID(), 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		throw std::runtime_error("Frame buffer is incomplete");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FBO::~FBO() {
	destroy();
}

void FBO::bind() const {
	glBindFramebuffer(GL_FRAMEBUFFER ,id);
	glViewport(0, 0, width, height);
}

void FBO::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::destroy() {
	if (id != 0) {
		glDeleteFramebuffers(1, &id);
		id = 0;
	}
}

const Texture& FBO::getTexture() const {
	return *colorAttachment;
}

FBO::FBO(FBO&& other) noexcept
	: id(other.id)
	, width(other.width)
	, height(other.height)
	, colorAttachment(std::move(other.colorAttachment))
{
	other.id = 0;
	other.width = 0;
	other.height = 0;
}

FBO& FBO::operator=(FBO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		colorAttachment = std::move(other.colorAttachment);
		width = other.width;
		height = other.height;

		other.id = 0;
	}

	return *this;
}

} // namespace Blackthorn::Graphics
