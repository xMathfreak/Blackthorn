#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Kinematics {
	glm::vec2 oldPosition{0, 0};
	glm::vec2 acceleration{0, 0};

	Kinematics() = default;
	Kinematics(float ox, float oy) : oldPosition(ox, oy) {}
	Kinematics(glm::vec2 oldPos) : oldPosition(oldPos) {}
};

} // namespace Blackthorn::ECS::Components