#include "Particles/Behaviors/Gravity.h"

namespace Blackthorn::Particles {

void Gravity::update(std::span<Particle> particles, float dt) {
	const glm::vec2 delta = acceleration * dt;

	for (Particle& p : particles)
		p.velocity += delta;
}

} // namespace Blackthorn::Particles
