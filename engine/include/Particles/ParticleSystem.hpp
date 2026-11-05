#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Export.h"
#include "Math/Random.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleEffectInstance.h"

namespace Blackthorn::Graphics {
	class Renderer;
}

namespace Blackthorn::Particles {

class ParticleRenderer;

/**
 * @brief Owns and drives every active particle effect instance for one scene.
 *
 * ParticleSystem only holds simulation state and does not own a ParticleRenderer. GPU
 * resources are shared at the device level through the scene context with a single
 * ParticleRenderer constructed by Engine alongside Graphics::Renderer and passed to
 * render() as needed.
 *
 * ParticleSystem is a per-scene, client-only member constructed fresh during each scene's
 * init(), ensuring particle effects do not persist across scene transitions and keeping
 * presentation only state out of ISimScene.
 *
 * @note Particles are to be explicitly stopped via stop() until @c ParticleEffect supports
 * finite duration or burst effects.
 *
 * @note Does not currently support draw call batching for same texture emitters.
 */
class BLACKTHORN_API ParticleSystem {
public:
	ParticleSystem() = default;

	/**
	 * @brief Spawns a new active instance of an effect at a world position.
	 * @param effect Effect definition to simulate. Must outlive the
	 * returned instance; this ParticleSystem does not check that.
	 * @param position World space coordinates offset by EmitterConfig::offset.
	 * @return Non-owning pointer to the spawned instance, valid until it is
	 * passed to stop().
	 *
	 * @note A ParticleEffectInstance will not automatically be removed if it
	 * currently has zero live particles.
	 */
	ParticleEffectInstance* spawn(const ParticleEffect& effect, glm::vec2 position);

	/**
	 * @brief Immediately removes an active instance.
	 * @param instance Pointer previously returned by spawn(). No-op if not
	 * currently active (e.g. already removed).
	 */
	void stop(ParticleEffectInstance* instance);

	/**
	 * @brief Advances every active instance by one frame.
	 *
	 * @note Does not auto-remove finished instances because isFinished() can
	 * also be true between spawns. Instances are removed exclusively via stop().
	 *
	 * @param dt Frame delta time, in seconds.
	 */
	void update(float dt);

	/**
	 * @brief Culls and draws every active instance's live particles into
	 * the given ParticleRenderer.
	 *
	 * For each emitter across all active instances, skips emitters with no live
	 * particles or texture, otherwise issuing one draw call per emitter with
	 * bounds-vs-view culling handled inside draw().
	 *
	 * @note Emitters are currently rendered individually.
	 *
	 * @param renderer Renderer to read the current view bounds from.
	 * @param particleRenderer Shared GPU-side renderer to draw into.
	 */
	void render(const Graphics::Renderer& renderer, ParticleRenderer& particleRenderer);

private:
	std::vector<std::unique_ptr<ParticleEffectInstance>> instances;
	Math::Random rng;
};

} // namespace Blackthorn::Particles
