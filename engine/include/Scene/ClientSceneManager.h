#pragma once

#include "Core/Export.h"
#include "Scene/IClientScene.h"
#include "Scene/SceneManager.h"

namespace Blackthorn::Scene {

/**
 * @brief Client-side scene manager that adds a `render()` dispatch step.
 *
 * Only compiled into `BlackthornEngine`. `Engine` constructs one of these
 * and stores it behind the `SceneManager` pointer inherited from
 * `EngineBase`, replacing the plain `SceneManager` created during
 * `EngineBase::init()`.
 *
 * `render()` walks the scene stack in the same render-visibility order as
 * the update walk, downcasting each scene to `IClientScene*`. Scenes that
 * are not `IClientScene` (e.g. a shared sim scene pushed onto a client
 * stack for testing) are silently skipped during render — they still
 * receive all simulation updates normally.
 *
 * The transition overlay callback is also dispatched here, so fade effects
 * still work correctly.
 */
class BLACKTHORN_API ClientSceneManager : public SceneManager {
public:
	ClientSceneManager() = default;
	~ClientSceneManager() override = default;

	/**
	 * @brief Renders visible client scenes in stack order.
	 *
	 * @param alpha Interpolation factor for smooth rendering between ticks.
	 */
	void render(float alpha) {
		if (scenes.empty())
			return;

		auto firstRender = scenes.begin();

		for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
			if ((*it)->blocksRender()) {
				firstRender = (it + 1).base();
				break;
			}
		}

		for (auto it = firstRender; it != scenes.end(); ++it) {
			auto* clientScene = dynamic_cast<IClientScene*>(it->get());

			if (clientScene)
				clientScene->render(alpha);
		}

		if (inTransition && transitionCallback) {
			float t = transitionTime / transitionDuration;
			transitionCallback(
				transitionPhase == TransitionPhase::FadeOut ? t : 1.0f - t
			);
		}
	}
};

} // namespace Blackthorn::Scene