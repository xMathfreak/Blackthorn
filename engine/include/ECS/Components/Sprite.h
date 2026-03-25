#pragma once

#include <glm/glm.hpp>

#include "Graphics/Texture.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Sprite {
	float width = 0.0f;
	float height = 0.0f;
	Graphics::Texture* texture = nullptr;

	float zOrder = 0.0f;

	Sprite() = default;
	Sprite(Graphics::Texture* tex, float w = 64.0f, float h = 64.0f)
		: width(w), height(h), texture(tex)
	{}
};

} // namespace Blackthorn::ECS::Components