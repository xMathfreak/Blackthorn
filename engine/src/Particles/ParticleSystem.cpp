#include "Particles/ParticleSystem.h"

#include "Graphics/Renderer.h"
#include "Particles/ParticleRenderer.h"

namespace Blackthorn::Particles {

ParticleEffectInstance* ParticleSystem::spawn(const ParticleEffect& effect, glm::vec2 position) {
	instances.push_back(std::make_unique<ParticleEffectInstance>(effect, position));
	return instances.back().get();
}

void ParticleSystem::stop(ParticleEffectInstance* instance) {
	std::erase_if(instances, [instance](const auto& owned) {
		return owned.get() == instance;
	});
}

void ParticleSystem::update(float dt) {
	for (auto& instance : instances)
		instance->update(dt, rng);
}

void ParticleSystem::render(const Graphics::Renderer& mainRenderer, ParticleRenderer& particleRenderer) {
	const SDL_FRect& viewBounds = mainRenderer.getViewBounds();
	particleRenderer.startBatch();

	for (const auto& instance : instances) {
		const auto configs = instance->getEffect().getEmitters();
		const auto runtimes = instance->getEmitterRuntimes();

		for (std::size_t i = 0; i < configs.size(); ++i) {
			const EmitterRuntime& runtime = runtimes[i];
			const EmitterConfig& config = configs[i];

			if (runtime.count == 0 || !config.texture)
				continue;

			particleRenderer.submit(
				instance->getParticles(static_cast<U32>(i)),
				*config.texture,
				config.blend,
				runtime.bounds,
				viewBounds
			);
		}
	}

	particleRenderer.endBatch();
}

} // namespace Blackthorn::Particles
