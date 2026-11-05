#pragma once

#include "Core/Export.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Particles {

/**
 * @brief Exponentially damps particle velocity over time.
 *
 * Stateless aside from its configuration, so it's safe to share across
 * every ParticleEffectInstance simulating the EmitterConfig that owns it.
 */
class BLACKTHORN_API Drag : public IBehavior {
public:
	/**
	 * @param coefficient Velocity falloff rate, per second. 0 disables drag
	 * entirely; larger values damp velocity more aggressively. Applied as
	 * `velocity *= max(0, 1 - coefficient * dt)` so a single large dt can't
	 * push velocity past zero and reverse it.
	 */
	explicit Drag(float coeff = 1.0f)
		: coefficient(coeff)
	{}

	void update(std::span<Particle> particles, float dt) override;

	/// Velocity falloff rate, per second.
	float coefficient;
};

} // namespace Blackthorn::Particles
