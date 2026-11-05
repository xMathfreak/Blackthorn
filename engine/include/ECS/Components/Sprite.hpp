#pragma once

#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include "Core/Export.h"

namespace Blackthorn {

namespace Graphics {
	class Texture;
}

namespace ECS::Components {

struct BLACKTHORN_API Sprite {
	float width = 0.0f;
	float height = 0.0f;
	Graphics::Texture* texture = nullptr;

	float zOrder = 0.0f;

	/// Source rect within texture, in pixels. Zero size (default) means "use the full texture".
	/// Written each fixedUpdate by Systems::AnimationSystem when a SpriteAnimation is present.
	SDL_FRect sourceRect{0, 0, 0, 0};

	Sprite() = default;
	Sprite(Graphics::Texture* tex, float w = 64.0f, float h = 64.0f)
		: width(w), height(h), texture(tex)
	{}
};

}

} // namespace Blackthorn::ECS::Components