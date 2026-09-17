#pragma once

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Types/Numeric.h"
#include "Math/Color.h"

namespace Blackthorn::Particles {

struct Particle {
	glm::vec2 position;
	glm::vec2 velocity;

	Math::Color color;

	float size;
	float rotation;

	float age;
	float lifetime;

	/// Source rect within EmitterConfig::texture, in pixels. Zero size
	/// means "use the full texture".
	/// Updated every frame by ParticleEffectInstance when EmitterConfig::clip is set;
	/// otherwise left zeroed at spawn and treated as full-texture by ParticleRenderer.
	SDL_FRect sourceRect;

	/// Index of the currently displayed frame within EmitterConfig::clip->frames.
	/// Only meaningful when EmitterConfig::clip is non-null.
	U32 currentFrame;

	/// Time accumulated within the current frame, in seconds. Only
	/// meaningful when EmitterConfig::clip is non-null.
	float frameElapsed;

	/// Direction for Animation::LoopMode::PingPong (+1 or -1). Only
	/// meaningful when EmitterConfig::clip is non-null.
	I8 pingPongDir;
};

} // namespace Blackthorn::Particles
