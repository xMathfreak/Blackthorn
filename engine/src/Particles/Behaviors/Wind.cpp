#include "Particles/Behaviors/Wind.h"

namespace Blackthorn::Particles {

void Wind::update(std::span<Particle> particles, float dt) {
	const glm::vec2 base = direction * strength;

	if (gustiness <= 0.0f) {
		const glm::vec2 delta = base * dt;

		for (Particle& p : particles)
			p.velocity += delta;

		return;
	}

	for (Particle& p : particles) {
		const float gust = noise.sample(glm::vec2(p.position.x * noiseScale, p.age * gustFrequency));
		p.velocity += base * (1.0f + gust * gustiness) * dt;
	}
}

} // namespace Blackthorn::Particles
