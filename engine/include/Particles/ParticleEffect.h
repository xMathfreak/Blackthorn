#pragma once

#include <span>
#include <vector>

#include "Core/Export.h"
#include "Particles/EmitterConfig.h"

namespace Blackthorn::Particles {

/**
 * @brief Static, shareable definition of a particle effect.
 *
 * A ParticleEffect is a set of EmitterConfigs that together make up
 * one visual effect (e.g. an explosion: flash + sparks + smoke + debris, each
 * as a separate emitter). It holds no runtime simulation state.
 * Spawn a ParticleEffectInstance from it to simulate and render particles.
 *
 * One ParticleEffect can back any number of simultaneously active
 * ParticleEffectInstances (e.g. several explosions using the same effect
 * definition at once),
 *
 * Move-only: EmitterConfig owns its behaviors via std::unique_ptr, so
 * ParticleEffect (and the vector of EmitterConfigs it holds) cannot be
 * copied, only moved.
 *
 * @note See @c IBehavior for statelessness requirement for behaviors.
 */
class BLACKTHORN_API ParticleEffect {
public:
	ParticleEffect() = default;

	explicit ParticleEffect(std::vector<EmitterConfig> emitterConfigs)
		: emitters(std::move(emitterConfigs))
	{}

	/**
	 * @brief Returns the emitter configs that make up this effect.
	 */
	[[nodiscard]] std::span<const EmitterConfig> getEmitters() const { return emitters; }

	/**
	 * @brief Adds an emitter configuration to this effect.
	 */
	void addEmitter(EmitterConfig emitter) { emitters.push_back(std::move(emitter)); }

private:
	std::vector<EmitterConfig> emitters;
};

} // namespace Blackthorn::Particles
