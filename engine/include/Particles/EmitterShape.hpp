#pragma once

#include <variant>

#include <glm/glm.hpp>

namespace Blackthorn::Particles {

struct Point {};

struct Line {
	float length = 1.0f;
};

struct Box {
	glm::vec2 size{1.0f};
};

using Rectangle = Box;

struct Circle {
	float radius = 1.0f;
};

using EmitterShape = std::variant<
	Point,
	Line,
	Box,
	Circle
>;

} // namespace Blackthorn::Particles