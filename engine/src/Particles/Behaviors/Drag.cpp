#include "Particles/Behaviors/Drag.h"

#include <algorithm>

namespace Blackthorn::Particles {

void Drag::update(std::span<Particle> particles, float dt) {
	const float factor = std::max(0.0f, 1.0f - coefficient * dt);

	for (Particle& p : particles)
		p.velocity *= factor;
}

} // namespace Blackthorn::Particles
