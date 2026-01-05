#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Core/Export.h"
#include "Scene/IScene.h"

namespace Blackthorn::Scene {

class BLACKTHORN_API SceneManager {
private:
	enum class TransitionPhase {
		FadeOut,
		FadeIn
	};

	std::vector<std::unique_ptr<IScene>> scenes;

	bool inTransition = false;
	TransitionPhase transitionPhase = TransitionPhase::FadeOut;
	std::unique_ptr<IScene> pendingScene;
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

public:
	SceneManager() = default;
	~SceneManager() = default;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	void pushScene(std::unique_ptr<IScene> scene) {
		if (!scene)
			return;

		if (!scenes.empty())
			scenes.back()->onPause();

		scene->sceneManager = this;
		scene->world = std::make_unique<ECS::World>();
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

	void changeScene(std::unique_ptr<IScene> scene) {
		if (!scene)
			return;

		clear();

		scene->sceneManager = this;
		scene->world = std::make_unique<ECS::World>();
		scene->onEnter();

		scenes.push_back(std::move(scene));
	}

	void clear() {
		while (!scenes.empty()) {
			scenes.back()->onExit();
			scenes.pop_back();
		}
	}

	void changeSceneWithTransition(std::unique_ptr<IScene> scene, std::function<void(float)> transition, float duration = 1.0f) {
		pendingScene = std::move(scene);
		transitionCallback = transition;
		transitionDuration = duration;
		transitionTime = 0.0f;
		inTransition = true;
		transitionPhase = TransitionPhase::FadeOut;
	}

	void fixedUpdate(float dt) {
		if (inTransition) {
			updateTransition(dt);
			return;
		}

		for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
			(*it)->fixedUpdate(dt);

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
			(*it)->render(alpha);
		}

		if (inTransition && transitionCallback) {
			float t = transitionTime / transitionDuration;
			if (transitionPhase == TransitionPhase::FadeOut) {
				transitionCallback(t);
			} else {
				transitionCallback(1.0f - t);
			}
		}
	}

	IScene* getCurrentScene() {
		return scenes.empty() ? nullptr : scenes.back().get();
	}

	const IScene* getCurrentScene() const {
		return scenes.empty() ? nullptr : scenes.back().get();
	}

	size_t getSceneCount() const {
		return scenes.size();
	}

	bool isEmpty() const {
		return scenes.empty();
	}
};

} // namespace Blackthorn::Scene