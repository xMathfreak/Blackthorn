#include "Graphics/FBO.h"

#include <stdexcept>

namespace Blackthorn::Graphics {

FBO::FBO(GLsizei w, GLsizei h) {
	allocate(w, h);
}

void FBO::allocate(GLsizei w, GLsizei h) {
	width = w;
	height = h;

	glGenFramebuffers(1, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, id);

	colorAttachment = std::make_unique<Texture>(width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorAttachment->getID(), 0);

	glGenRenderbuffers(1, &depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		glDeleteBuffers(1, &depthRBO);
		glDeleteFramebuffers(1, &id);

		depthRBO = 0;
		id = 0;

		throw std::runtime_error("FBO incomplete (status 0x)" + std::to_string(status) + ')');
	}
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
	colorAttachment.reset();

	if (depthRBO != 0) {
		glDeleteRenderbuffers(1, &depthRBO);
		depthRBO = 0;
	}

	if (id != 0) {
		glDeleteFramebuffers(1, &id);
		id = 0;
	}

	width = 0;
	height = 0;
}

const Texture& FBO::getTexture() const {
	return *colorAttachment;
}

FBO::FBO(FBO&& other) noexcept
	: id(other.id)
	, width(other.width)
	, height(other.height)
	, depthRBO(other.depthRBO)
	, colorAttachment(std::move(other.colorAttachment))
{
	other.id = 0;
	other.depthRBO = 0;
	other.width = 0;
	other.height = 0;
}

FBO& FBO::operator=(FBO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		depthRBO = other.depthRBO;
		width = other.width;
		height = other.height;
		colorAttachment = std::move(other.colorAttachment);

		other.id = 0;
		other.depthRBO = 0;
		other.width = 0;
		other.height = 0;
	}
	return *this;
}

void FBO::resize(GLsizei w, GLsizei h) {
	if (w == width && h == height)
		return;

	destroy();
	allocate(w, h);
}

} // namespace Blackthorn::Graphics
