#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Particles {

/**
 * @brief Applies a constant acceleration to every particle's velocity.
 *
 * Typically used for downward gravity, but `acceleration` may point in any
 * direction (e.g. to simulate a directional force field). Stateless aside
 * from its configuration, so it's safe to share across every
 * ParticleEffectInstance simulating the EmitterConfig that owns it.
 */
class BLACKTHORN_API Gravity : public IBehavior {
public:
	/**
	 * @param acceleration Acceleration applied every frame, in units/second^2.
	 */
	explicit Gravity(glm::vec2 acc = {0.0f, 9.81f})
		: acceleration(acc)
	{}

	void update(std::span<Particle> particles, float dt) override;

	/// Acceleration applied every frame, in units/second^2.
	glm::vec2 acceleration;
};

} // namespace Blackthorn::Particles
