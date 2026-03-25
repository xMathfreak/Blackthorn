#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Transform {
	glm::vec2 position{0, 0};
	float angle = 0.0f;
	float scale = 1.0f;

	Transform() = default;
	Transform(float x, float y) : position(x, y) {}
	Transform(glm::vec2 pos) : position(pos) {}
};

} // namespace Blackthorn::ECS::Components