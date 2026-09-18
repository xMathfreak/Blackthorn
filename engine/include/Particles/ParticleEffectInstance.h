#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Math/Random.h"
#include "Particles/Particle.h"
#include "Particles/ParticleEffect.h"

namespace Blackthorn::Particles {

/**
 * @brief Per-emitter runtime state within a ParticleEffectInstance.
 *
 * Indices are parallel to ParticleEffect::getEmitters(); the Nth
 * EmitterRuntime corresponds to the Nth EmitterConfig in the owning effect.
 *
 * `begin` and `capacity` are fixed for the lifetime of the owning
 * ParticleEffectInstance (set once at construction from
 * EmitterConfig::resolveCapacity()) so that ranges never move relative to
 * one another. Emission and death only ever touch this emitter's own
 * `[begin, begin + count)` slice of the shared particle buffer, with no
 * compaction pass needed across emitters.
 */
struct BLACKTHORN_API EmitterRuntime {
	/// Fixed start offset of this emitter's range within
	/// ParticleEffectInstance's particle buffer. Never changes after construction.
	U32 begin = 0;

	/// Number of currently-live particles in this emitter's range.
	U32 count = 0;

	/// Reserved size of this emitter's range. `count` never exceeds this;
	/// emission beyond capacity is dropped for the frame it would occur in.
	U32 capacity = 0;

	/// Accumulates fractional particle spawns between update() calls
	/// (EmitterConfig::rate is particles/second; update() runs once/frame).
	/// Clamped to avoid unbounded buildup while the emitter is at capacity.
	float emissionAccumulator = 0.0f;

	/// World-space AABB of this emitter's currently-live particles,
	/// recomputed every update(). {0,0,0,0} when count == 0.
	SDL_FRect bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Live simulation state for one active instance of a ParticleEffect.
 *
 * Owns the actual particle pool and per-emitter runtime state (emission
 * accumulators, live counts, tracked bounds). The referenced ParticleEffect
 * is not owned and must outlive this instance; effects are pure
 * configuration and may be shared by many simultaneously active instances.
 *
 * Particle storage uses fixed, non-overlapping per-emitter ranges within a
 * single contiguous buffer (see EmitterRuntime) so that spawning/killing
 * particles for one emitter never shifts another emitter's range, and so a
 * std::span<Particle> handed to an IBehavior::update() call for one emitter
 * stays valid for the remainder of the frame.
 */
class BLACKTHORN_API ParticleEffectInstance {
public:
	/**
	 * @brief Constructs an instance from an effect definition, placed at a
	 * world position.
	 *
	 * Sizes the particle buffer once, up front, to the sum of every
	 * emitter's resolved capacity (EmitterConfig::resolveCapacity()). The
	 * buffer is never resized afterwards.
	 *
	 * @param effect Effect definition to simulate. Must outlive this instance.
	 * @param wpos Where this instance is playing in the world. Added on top
	 * of each emitter's own EmitterConfig::offset (an emitter-local offset)
	 * when sampling spawn positions.
	 */
	ParticleEffectInstance(const ParticleEffect& effect, glm::vec2 wpos);

	/**
	 * @brief Moves where this instance spawns new particles from.
	 *
	 * Affects only particles emitted after this call. Already-live
	 * particles keep whatever position they were spawned at and continue
	 * their own trajectory unaffected.
	 *
	 * @param wpos New world position for future spawns.
	 */
	void setPosition(glm::vec2 wpos) { worldPosition = wpos; }

	/**
	 * @brief Returns the world position new particles currently spawn from.
	 * See setPosition()'s doc comment for what this does and doesn't affect.
	 */
	[[nodiscard]] glm::vec2 getPosition() const { return worldPosition; }

	/**
	 * @brief Advances the simulation by one frame.
	 *
	 * For each emitter, in order: applies its behaviors to its live
	 * particle range, integrates position from velocity, ages and removes
	 * expired particles (swap-and-pop within the emitter's own range),
	 * emits new particles per its emission accumulator (up to its
	 * capacity), and recomputes its tracked bounds.
	 *
	 * @param dt  Frame delta time, in seconds.
	 * @param rng Random source used for spawn position/speed/lifetime sampling.
	 */
	void update(float dt, Math::Random& rng);

	/**
	 * @brief Returns the owning effect definition.
	 */
	[[nodiscard]] const ParticleEffect& getEffect() const { return *effect; }

	/**
	 * @brief Returns per-emitter runtime state, parallel to
	 * `getEffect().getEmitters()`.
	 */
	[[nodiscard]] std::span<const EmitterRuntime> getEmitterRuntimes() const { return emitters; }

	/**
	 * @brief Returns the live particle span for one emitter.
	 * @param emitterIndex Index into getEffect().getEmitters() / getEmitterRuntimes().
	 */
	[[nodiscard]] std::span<const Particle> getParticles(U32 emitterIndex) const;

	/**
	 * @brief Whether every emitter in this instance currently has no live
	 * particles.
	 *
	 * @note ParticleEffect does not expose a looping/duration concept,
	 * so every emitter here emits indefinitely. This only goes true once
	 * the caller stops calling update() (e.g. after removing the emitters'
	 * configs, or simply discarding this instance) and the last particles
	 * age out. It is not, on its own, a "this effect played once and is
	 * done" signal yet.
	 */
	[[nodiscard]] bool isFinished() const;

private:
	const ParticleEffect* effect;

	/// Where new particles currently spawn from.
	// Mutable, but only affects future emissions.
	glm::vec2 worldPosition;

	/// Shared particle buffer. Sized once at construction; never resized.
	std::vector<Particle> particles;

	/// Parallel to effect->getEmitters().
	std::vector<EmitterRuntime> emitters;

	void updateEmitter(const EmitterConfig& config, EmitterRuntime& runtime, float dt, Math::Random& rng);
};

} // namespace Blackthorn::Particles
