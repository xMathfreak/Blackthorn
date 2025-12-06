#include "Graphics/GlobalUBO.h"

#ifdef BLACKTHORN_DEBUG
	#include <SDL3/SDL.h>
#endif

namespace Blackthorn::Graphics {

GlobalUBO::GlobalUBO() {
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalData), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("GlobalUBO created (ID: %u)", ubo);
	#endif
}

GlobalUBO::~GlobalUBO() {
	if (ubo != 0)
		glDeleteBuffers(1, &ubo);
}

void GlobalUBO::bind(GLuint bindingPoint) {
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
}

void GlobalUBO::updateViewProjection(const glm::mat4& viewProj) {
	data.viewProjection = viewProj;

	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &data.viewProjection);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GlobalUBO::updateData(const GlobalData& newData) {
	data = newData;

	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalData), &data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace Blackthorn::Graphics
