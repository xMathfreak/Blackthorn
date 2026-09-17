#pragma once

#include <memory>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Math/NumericRange.h"
#include "Math/Random.h"
#include "Particles/EmitterShape.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Graphics {
	class Texture;
}

namespace Blackthorn::Animation {
	class SpriteClip;
}

namespace Blackthorn::Particles {

/**
 * @brief Controls how a position is sampled from an EmitterShape.
 */
enum class Distribution {
	/// Uniformly sampled from the shape's interior (or its full length, for Line).
	Random,
	/// Sampled from the shape's edge/perimeter only.
	Boundary
};

/// @name Shape sampling
/// Free functions implementing Distribution-aware sampling for each
/// alternative in EmitterShape. Dispatched via std::visit from
/// EmitterConfig::samplePosition().
/// @{
glm::vec2 sample(const Point&, Distribution, Math::Random&);
glm::vec2 sample(const Line&, Distribution, Math::Random&);
glm::vec2 sample(const Box&, Distribution, Math::Random&);
glm::vec2 sample(const Circle&, Distribution, Math::Random&);
/// @}

/** @brief Randomly sampled emission data. */
struct EmissionSample {
	glm::vec2 position;
	float rotation;
};

/**
 * @brief Configuration for a single particle emitter.
 *
 * Defines particles spawn rate/shape/lifetime/speed, appearance and behaviors.
 * Holds no runtime simulation state so a single EmitterConfig can be shared by any
 * number of simultaneously active effect instances.
 *
 * A ParticleEffect groups one or more EmitterConfigs together to form a complete
 * effect (e.g an explosion consisting of flash + sparks + smoke + debris).
 *
 * @note Because EmitterConfig can back many simultaneously active
 * instances, behaviors must stay free of per-simulation mutable state.
 */
struct BLACKTHORN_API EmitterConfig {
	/// How spawn positions are distributed across `shape`.
	Distribution distribution = Distribution::Random;

	/// Spawn area/line/point particles are sampled from, in emitter-local space.
	EmitterShape shape = Point();

	/// Particles spawned per second.
	float rate = 10.0f;

	/// Particle lifetime range, in seconds. Sampled per-particle at spawn time.
	Math::FloatRange lifetimeRange{1.0f, 10.0f};

	/// Particle speed range, in units/second. Sampled per-particle at spawn time.
	Math::FloatRange speedRange{1.0f};

	/// Particle size range, in pixels (quad side length). Sampled
	/// per-particle at spawn time.
	Math::FloatRange sizeRange{5.0f};

	/// Emitter rotation range, in radians. Rotates both the sampled spawn shape
	/// and the initial velocity direction of spawned particles.
	/// Defaults to a range between 0 and 2 * PI radians.
	Math::FloatRange rotation{0.0f, 2 * std::numbers::pi_v<float>};

	/// Emitter offset, relative to wherever the owning ParticleEffectInstance
	/// is placed in the world.
	/// e.g. offsetting a "debris" emitter slightly from an explosion
	/// effect's "flash" emitter.
	glm::vec2 offset{0.0f};

	/// Texture drawn for particles spawned by this emitter.
	///
	/// Prefer obtaining this from an Assets::AssetHandle<Graphics::Texture>
	/// (e.g. a handle returned by AssetManager::load<Texture>() and kept
	/// alive elsewhere) rather than any other raw pointer.
	/// EmitterConfig itself does not keep the asset resident.
	Graphics::Texture* texture = nullptr;

	/// Animation clip driving per-particle sprite-sheet playback, or nullptr
	/// for a static (non-animated) texture.
	///
	/// Like `texture`, the clip must outlive this config and, transitively,
	/// every ParticleEffectInstance simulating it.
	const Animation::SpriteClip* clip = nullptr;

	/// Behaviors applied, in order, to this emitter's live particle set
	/// every frame (e.g. Wind, Gravity, Drag). Each behavior operates on
	/// the full particle span for one update() call rather than per
	/// particle.
	std::vector<std::unique_ptr<IBehavior>> behaviors{};

	/// Maximum simultaneously-live particles for this emitter. 0 (default)
	/// derives the capacity automatically from `rate` and `lifetimeRange`
	/// via resolveCapacity(). Set explicitly to override (e.g. to cap
	/// below the natural steady-state count).
	U32 maxParticles = 0;

	/**
	 * @brief Samples an emitter-local spawn position and rotation for a new particle.
	 * @param rng Random source to sample from.
	 * @return A position and rotation sampled from `shape` (per `distribution`)
	 * and offset by `offset`, all relative to wherever the
	 * owning ParticleEffectInstance is placed in the world.
	 */
	[[nodiscard]] EmissionSample sampleEmission(Math::Random& rng) const;

	/**
	 * @brief Resolves the maximum simultaneously-live particle count for
	 * this emitter.
	 *
	 * Returns `maxParticles` directly if it has been set explicitly
	 * (non-zero). Otherwise derives the steady-state capacity: the maximum
	 * number of particles that could be alive at once given `rate` and the
	 * longest possible lifetime, i.e. `ceil(rate * lifetimeRange.maxVal)`.
	 *
	 * Used by ParticleEffectInstance to size each emitter's fixed,
	 * contiguous particle range once, at construction.
	 */
	[[nodiscard]] U32 resolveCapacity() const;
};

} // namespace Blackthorn::Particles
