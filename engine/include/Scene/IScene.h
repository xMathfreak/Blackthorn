#pragma once

#include "Audio/AudioManager.h"
#include "Core/Export.h"
#include "Graphics/Renderer.h"
#include "Input/InputManager.h"
#include "Particles/ParticleSystem.h"
#include "Scene/ISceneContext.h"
#include "Scene/ISimScene.h"
#include "UI/UIManager.h"

namespace Blackthorn::Scene {

/**
 * @brief Client-side scene: a headless `ISimScene` plus rendering, audio,
 * and UI.
 *
 * @details
 * `IScene` is the type most game code should subclass - it is the "full"
 * scene available in the graphics-enabled client build (`Engine`), adding
 * a `render()` step, a `UI::UIManager`, and a `Particles::ParticleSystem`
 * on top of everything `ISimScene` already provides.
 *
 * Scenes that must also run on a headless dedicated server (no rendering,
 * audio, UI, or particles) should derive from `ISimScene` (declared in
 * `Scene/ISimScene.h`) instead.
 *
 * @par Construction
 * Like `ISimScene`, subclasses do not need to accept or forward a context:
 * @code
 * class MainMenuScene : public IScene {
 * public:
 *     MainMenuScene() = default;
 *
 *     void onEnter() override {
 *         assets().loadAsync<Graphics::Texture>(...);
 *     }
 *
 *     void render(float alpha) override {
 *         IScene::render(alpha);
 *         renderer().drawQuad(...);
 *     }
 * };
 * @endcode
 * The context is injected automatically by `SceneManager` (specifically
 * `ClientSceneManager` in the client build) immediately before `init()`.
 *
 * @par Accessing engine services
 * In addition to the forwarding methods inherited from `ISimScene`
 * (`assets()`, `jobs()`, `simClock()`, ...), this class adds `renderer()`,
 * `audio()`, and `input()` so client-only services are just as easy to
 * reach without going through `getContext()`.
 *
 * @par Particles
 * `particleSystem` is owned here, constructed fresh in init()
 * the same way `uiManager` is, rather than living on `ISceneContext`
 * alongside the renderer. Only the GPU-side `Particles::ParticleRenderer`
 * (VAO/VBO/shader) is shared engine-wide via `ISceneContext`; which
 * particle effects are currently playing is scene state, so effects
 * spawned by one scene are torn down along with it rather than lingering
 * into whatever scene comes next.
 */
class BLACKTHORN_API IScene : public ISimScene {
protected:
	std::unique_ptr<UI::UIManager> uiManager;
	std::unique_ptr<Particles::ParticleSystem> particleSystem;

public:
	IScene() = default;
	~IScene() override = default;

	void init() override {
		ISimScene::init();
		uiManager = std::make_unique<UI::UIManager>();
		particleSystem = std::make_unique<Particles::ParticleSystem>();
	}

	void update(float dt) override {
		ISimScene::update(dt);

		if (particleSystem)
			particleSystem->update(dt);

		if (uiManager) {
			uiManager->update(dt);
			uiManager->handleInput(input());
		}
	}

	virtual void render(float alpha) {
		if (world)
			world->render(alpha);

		if (particleSystem)
			particleSystem->render(renderer(), getContext().getParticleRenderer());

		if (uiManager)
			uiManager->render(renderer());
	}

	UI::UIManager* getUIManager() { return uiManager.get(); }
	const UI::UIManager* getUIManager() const { return uiManager.get(); }

	Particles::ParticleSystem* getParticleSystem() { return particleSystem.get(); }
	const Particles::ParticleSystem* getParticleSystem() const { return particleSystem.get(); }

	/**
	 * @brief Returns the full client scene context, including rendering,
	 * audio, and input access.
	 *
	 * Hides `ISimScene::getContext()` with the more specific type. Safe to
	 * downcast because `SceneManager::pushScene()`/`changeScene()` only ever
	 * inject an `ISceneContext` when managing `IScene` instances (see
	 * `ClientSceneManager`).
	 */
	ISceneContext& getContext() {
		return static_cast<ISceneContext&>(ISimScene::getContext());
	}

	const ISceneContext& getContext() const {
		return static_cast<const ISceneContext&>(ISimScene::getContext());
	}

	/** @brief Shortcut for `getContext().getRenderer()`. */
	Graphics::Renderer& renderer() { return getContext().getRenderer(); }

	/** @brief Shortcut for `getContext().getAudioManager()`. */
	Audio::AudioManager& audio() { return getContext().getAudioManager(); }

	/** @brief Shortcut for `getContext().getInputManager()`. */
	Input::InputManager& input() { return getContext().getInputManager(); }

	/** @brief Shortcut for this scene's owned Particles::ParticleSystem. */
	Particles::ParticleSystem& particles() { return *particleSystem; }
};

} // namespace Blackthorn::Scene
