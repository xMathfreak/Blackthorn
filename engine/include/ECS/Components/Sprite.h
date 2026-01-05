#pragma once

#include <SDL3/SDL.h>

#include "Graphics/Texture.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Sprite {
	SDL_FRect src{0, 0, 64, 64};
	SDL_FRect dest{0, 0, 64, 64};
	Graphics::Texture* texture = nullptr;

	float zOrder = 0.0f;
	
	bool flipX = false;
	bool flipY = false;


	BLACKTHORN_API Sprite() = default;
	BLACKTHORN_API Sprite(Graphics::Texture* tex, float w = 64.0f, float h = 64.0f)
		: texture(tex)
	{
		src.w = dest.w = w;
		src.h = dest.h = h;
	}
};

} // namespace Blackthorn::ECS::Components