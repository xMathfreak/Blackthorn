#pragma once

#include "Core/Export.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Blackthorn::Graphics {

class BLACKTHORN_API GlobalUBO {
private:
	GLuint ubo = 0;

	struct GlobalData {
		alignas(16) glm::mat4 viewProjection;
	};

	GlobalData data;

public:
	GlobalUBO();
	~GlobalUBO();

	GlobalUBO(const GlobalUBO&) = delete;
	GlobalUBO& operator=(const GlobalUBO&) = delete;

	void bind(GLuint bindingPoint = 0);
	void updateViewProjection(const glm::mat4& viewProj);
	void updateData(const GlobalData& newData);
};

} // namespace Blackthorn::Graphics
