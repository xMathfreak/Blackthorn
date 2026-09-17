#pragma once

#include <span>

#include "Core/Export.h"
#include "Particles/Particle.h"

namespace Blackthorn::Particles {

/**
 * @brief Interface for a per-frame force/mutation applied to an emitter's
 * live particle set.
 *
 * Behaviors operate on an emitter's full particle span in one call rather
 * than being invoked per particle (e.g. one update() call for 10,000
 * particles, not 10,000 calls). Implementations should loop over
 * @p particles internally.
 *
 * @note Implementations are owned by EmitterConfig, which is shared,
 * static configuration that may back many simultaneously active
 * ParticleEffectInstances at once. A behavior instance must therefore stay
 * free of per-simulation mutable state (e.g. an elapsed-time clock). It
 * should behave as a pure function of the particles/dt it's given (and any
 * per-particle state such as Particle::age), not accumulate its own. See
 * Behaviors/Wind.h for an example of deriving time-varying behavior from
 * Particle::age instead of an internally-owned clock.
 */
class BLACKTHORN_API IBehavior {
public:
	virtual ~IBehavior() = default;

	/**
	 * @brief Applies this behavior to a set of live particles.
	 * @param particles Particles belonging to a single emitter this frame.
	 * @param dt Frame delta time, in seconds.
	 */
	virtual void update(std::span<Particle> particles, float dt) = 0;
};

} // namespace Blackthorn::Particles
