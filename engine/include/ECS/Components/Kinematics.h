#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Kinematics {
	glm::vec2 oldPosition{0, 0};
	glm::vec2 acceleration{0, 0};

	BLACKTHORN_API Kinematics() = default;
	BLACKTHORN_API Kinematics(float ox, float oy) : oldPosition(ox, oy) {}
	BLACKTHORN_API Kinematics(glm::vec2 oldPos) : oldPosition(oldPos) {}
};

} // namespace Blackthorn::ECS::Components