#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Debug/Logger.h"
#include "Scene/ISimScene.h"

namespace Blackthorn::Scene {

/**
 * @brief Simulation-only scene stack manager.
 *
 * Drives `fixedUpdate`, `update`, and `lateUpdate` across the scene stack.
 * No render step - that is provided by `ClientSceneManager` in the client
 * build.
 *
 * Used directly by `EngineBase` and the dedicated server. `Engine` replaces
 * the `EngineBase` instance with a `ClientSceneManager` at init time.
 *
 * @par Context injection
 * `SceneManager` is responsible for handing every scene its `ISimContext`
 * automatically. Call `setContext()` once, right after the engine's
 * context object exists, before pushing any scenes:
 * @code
 * sceneManager = std::make_unique<Scene::SceneManager>();
 * simContext = std::make_unique<Scene::SimContextImpl>(..., *sceneManager, ...);
 * sceneManager->setContext(*simContext);
 * @endcode
 * From then on, `pushScene()` and `changeScene()` inject the context into
 * each scene automatically before calling `init()`. Scene subclasses never
 * need to know about this - see `ISimScene`.
 */
class BLACKTHORN_API SceneManager {
protected:
	enum class TransitionPhase {
		FadeOut,
		FadeIn
	};

	std::vector<std::unique_ptr<ISimScene>> scenes;

	// Injected once via setContext(), then handed to every pushed scene.
	ISimContext* context = nullptr;

	bool inTransition = false;
	TransitionPhase transitionPhase = TransitionPhase::FadeOut;
	std::unique_ptr<ISimScene> pendingScene;
	std::function<void(float)> transitionCallback;
	float transitionDuration = 0.0f;
	float transitionTime = 0.0f;

	void updateTransition(float dt) {
		transitionTime += dt;

		if (transitionTime >= transitionDuration) {
			if (transitionPhase == TransitionPhase::FadeOut) {
				changeScene(std::move(pendingScene));
				transitionPhase = TransitionPhase::FadeIn;
				transitionTime = 0.0f;
			} else {
				inTransition = false;
				transitionCallback = nullptr;
			}
		}
	}

	/**
	 * @brief Injects the active context into @p scene. Logs and returns
	 * false if `setContext()` has not been called yet.
	 */
	bool injectContext(ISimScene& scene) {
		if (!context) {
			BT_ERROR(
				"SceneManager: cannot push scene '{}', SceneManager::setContext() "
				"was never called",
				scene.getName()
			);

			return false;
		}

		scene.setContext(*context);
		return true;
	}

public:
	SceneManager() = default;
	virtual ~SceneManager() = default;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	/**
	 * @brief Sets the simulation context handed to every scene from now on.
	 *
	 * Must be called once before the first `pushScene()`/`changeScene()`.
	 * Called by `EngineCore`/`Engine` immediately after the engine's context
	 * object is constructed. Not intended to be called by game code.
	 *
	 * @param ctx Context to inject into scenes. Must outlive this manager.
	 */
	void setContext(ISimContext& ctx) { context = &ctx; }

	void pushScene(std::unique_ptr<ISimScene> scene) {
		if (!scene || !injectContext(*scene))
			return;

		if (!scenes.empty())
			scenes.back()->onPause();

		scene->init();
		scene->onEnter();

		scenes.push_back(std::move(scene));
	}

	void popScene() {
		if (scenes.empty())
			return;

		scenes.back()->onExit();
		scenes.pop_back();

		if (!scenes.empty())
			scenes.back()->onResume();
	}

	void changeScene(std::unique_ptr<ISimScene> scene) {
		if (!scene || !injectContext(*scene))
			return;

		clear();

		scene->init();
		scene->onEnter();

		scenes.push_back(std::move(scene));
	}

	void clear() {
		while (!scenes.empty()) {
			scenes.back()->onExit();
			scenes.pop_back();
		}
	}

	void changeSceneWithTransition(
		std::unique_ptr<ISimScene> scene,
		std::function<void(float)> transition,
		float duration = 1.0f
	) {
		pendingScene = std::move(scene);
		transitionCallback = std::move(transition);
		transitionDuration = duration;
		transitionTime = 0.0f;
		inTransition = true;
		transitionPhase = TransitionPhase::FadeOut;
	}

	void fixedUpdate(float dt, U64 tick) {
		if (inTransition)
			return;

		for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
			(*it)->fixedUpdate(dt, tick);

			if ((*it)->blocksUpdate())
				break;
		}
	}

	void update(float dt) {
		if (inTransition) {
			updateTransition(dt);
			return;
		}

		for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
			(*it)->update(dt);

			if ((*it)->blocksUpdate())
				break;
		}
	}

	void lateUpdate(float dt) {
		if (inTransition)
			return;

		for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
			(*it)->lateUpdate(dt);

			if ((*it)->blocksUpdate())
				break;
		}
	}

	ISimScene* getCurrentScene() {
		return scenes.empty() ? nullptr : scenes.back().get();
	}

	const ISimScene* getCurrentScene() const {
		return scenes.empty() ? nullptr : scenes.back().get();
	}

	size_t getSceneCount() const { return scenes.size(); }
	bool isEmpty() const { return scenes.empty(); }
};

} // namespace Blackthorn::Scene
