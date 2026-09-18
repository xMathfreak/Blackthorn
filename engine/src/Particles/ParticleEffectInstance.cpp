#include "Particles/ParticleEffectInstance.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Animation/SpriteClipPlayback.h"
#include "Math/Color.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Particles {

ParticleEffectInstance::ParticleEffectInstance(const ParticleEffect& effectRef, glm::vec2 wpos)
	: effect(&effectRef)
	, worldPosition(wpos)
{
	const auto configs = effect->getEmitters();

	emitters.resize(configs.size());

	U32 totalCapacity = 0;
	for (std::size_t i = 0; i < configs.size(); ++i) {
		const U32 capacity = configs[i].resolveCapacity();

		emitters[i].begin = totalCapacity;
		emitters[i].capacity = capacity;

		totalCapacity += capacity;
	}

	particles.resize(totalCapacity);
}

std::span<const Particle> ParticleEffectInstance::getParticles(U32 emitterIndex) const {
	const EmitterRuntime& runtime = emitters[emitterIndex];
	return std::span<const Particle>(particles.data() + runtime.begin, runtime.count);
}

bool ParticleEffectInstance::isFinished() const {
	return std::ranges::all_of(emitters, [](const EmitterRuntime& e) {
		return e.count == 0;
	});
}

void ParticleEffectInstance::update(float dt, Math::Random& rng) {
	const auto configs = effect->getEmitters();

	for (std::size_t i = 0; i < configs.size(); ++i)
		updateEmitter(configs[i], emitters[i], dt, rng);
}

void ParticleEffectInstance::updateEmitter(const EmitterConfig& config, EmitterRuntime& runtime, float dt, Math::Random& rng) {
	// Behaviors operate on the full live set at once.
	{
		std::span<Particle> live(particles.data() + runtime.begin, runtime.count);

		for (const auto& behavior : config.behaviors)
			behavior->update(live, dt);
	}

	// Apply motion, age and culling within emitter's own range.
	glm::vec2 boundsMin(std::numeric_limits<float>::max());
	glm::vec2 boundsMax(std::numeric_limits<float>::lowest());

	for (U32 i = 0; i < runtime.count; ) {
		Particle& p = particles[runtime.begin + i];

		p.position += p.velocity * dt;
		p.age += dt;

		if (p.age >= p.lifetime) {
			// Move the last live particle into the dead slot, shrink the range
			// and re-check the same slot on the next iteration because it now
			// contains an unprocessed particle.
			particles[runtime.begin + i] = particles[runtime.begin + runtime.count - 1];
			--runtime.count;
			continue;
		}

		if (config.clip && config.clip->isValid()) {
			bool playing = true;
			Animation::advanceClip(*config.clip, p.currentFrame, p.frameElapsed, p.pingPongDir, playing, dt);
			p.sourceRect = config.clip->frames[p.currentFrame].sourceRect;
		}

		boundsMin = glm::min(boundsMin, p.position);
		boundsMax = glm::max(boundsMax, p.position);

		++i;
	}

	// Emit new particles up for this frame's dt, up to capacity
	runtime.emissionAccumulator += config.rate * dt;

	while (runtime.emissionAccumulator >= 1.0f && runtime.count < runtime.capacity) {
		runtime.emissionAccumulator -= 1.0f;

		Particle& p = particles[runtime.begin + runtime.count];

		const float speed = config.speedRange.sample(rng);

		auto sampled = config.sampleEmission(rng);

		p.position = worldPosition + sampled.position;
		p.velocity = { std::cos(sampled.rotation) * speed, std::sin(sampled.rotation) * speed };
		p.color = Math::Colors::White;
		p.size = config.sizeRange.sample(rng);
		p.rotation = sampled.rotation;
		p.age = 0.0f;
		p.lifetime = config.lifetimeRange.sample(rng);
		p.sourceRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
		p.currentFrame = 0;
		p.frameElapsed = 0.0f;
		p.pingPongDir = 1;

		if (config.clip && config.clip->isValid())
			p.sourceRect = config.clip->frames[0].sourceRect;

		boundsMin = glm::min(boundsMin, p.position);
		boundsMax = glm::max(boundsMax, p.position);

		++runtime.count;
	}

	// Clamp to prevent burst of particles on when slots get freed.
	if (runtime.count >= runtime.capacity)
		runtime.emissionAccumulator = std::min(runtime.emissionAccumulator, 1.0f);

	runtime.bounds = (runtime.count == 0)
		? SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f}
		: SDL_FRect{
			boundsMin.x, boundsMin.y,
			boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y
		};
}

} // namespace Blackthorn::Particles
