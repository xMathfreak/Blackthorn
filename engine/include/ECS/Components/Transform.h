#pragma once

#include "Core/Export.h"

#include <glm/glm.hpp>

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Transform {
	glm::vec2 position{0, 0};
	float angle = 0.0f;
	float scale = 1.0f;

	BLACKTHORN_API Transform() = default;
	BLACKTHORN_API Transform(float x, float y) : position(x, y) {}
	BLACKTHORN_API Transform(glm::vec2 pos) : position(pos) {} 
};

} // namespace Blackthorn::ECS::Components