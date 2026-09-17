#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"
#include "Math/Perlin.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Particles {

/**
 * @brief Applies a directional acceleration with optional gust variation.
 *
 * With `gustiness == 0`, this behaves identically to Gravity pointed in an
 * arbitrary direction. Setting `gustiness > 0` blends in a Perlin-noise
 * field, sampled per-particle from (position, age), so particles drift with
 * some variation instead of moving in perfect lockstep.
 *
 * @note The noise field is sampled using each particle's own
 * Particle::age as its time axis rather than a Wind-owned elapsed-time
 * clock. This keeps Wind fully stateless with respect to simulation time
 * (its Math::Noise::Perlin member is an immutable lookup table generated
 * once at construction, not mutated by sample() calls) because a single
 * Wind instance, owned by an EmitterConfig, may be driving particles across
 * several simultaneously active ParticleEffectInstances at once.
 */
class BLACKTHORN_API Wind : public IBehavior {
public:
	/**
	 * @param direction Normalized wind direction. Magnitude should be expressed via `strength` instead.
	 * @param strength Base acceleration magnitude, in units/second^2.
	 * @param gustiness Fraction of `strength` the noise field may add or
	 * subtract. 0 (default) disables gusts entirely.`direction * strength`
	 * is applied uniformly to every particle.
	 * @param noiseScale Spatial frequency of the noise field, applied to
	 * particle position.
	 * @param gustFrequency Temporal frequency of the noise field, applied
	 * to particle age.
	 */
	explicit Wind(
		glm::vec2 dir = {1.0f, 0.0f},
		float str = 1.0f,
		float gust = 0.0f,
		float noiseSc = 0.5f,
		float gustFreq = 1.0f
	)
		: direction(dir)
		, strength(str)
		, gustiness(gust)
		, noiseScale(noiseSc)
		, gustFrequency(gustFreq)
	{}

	void update(std::span<Particle> particles, float dt) override;

	/// Wind direction. Expected to be normalized.
	glm::vec2 direction;

	/// Base acceleration magnitude, in units/second^2.
	float strength;

	/// Fraction of `strength` the noise field may add or subtract. 0 disables gusts.
	float gustiness;

	/// Spatial frequency of the noise field, applied to particle position.
	float noiseScale;

	/// Temporal frequency of the noise field, applied to particle age.
	float gustFrequency;

private:
	/// Fixed noise permutation table generated once at construction.
	Math::Noise::Perlin noise;
};

} // namespace Blackthorn::Particles
