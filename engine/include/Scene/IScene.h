#pragma once

#include "Core/Export.h"
#include "ECS/World.h"

namespace Blackthorn::Scene {

class SceneManager;

class BLACKTHORN_API IScene {
protected:
	std::unique_ptr<ECS::World> world;
	SceneManager* sceneManager = nullptr;

	friend class SceneManager;

public:
	virtual ~IScene() = default;
	
	virtual void onEnter() {}
	virtual void onExit() {}
	virtual void onPause() {}
	virtual void onResume() {}

	virtual bool blocksUpdate() const { return true; }
	virtual bool blocksRender() const { return true; }

	virtual void fixedUpdate(float dt) {
		if (world)
			world->fixedUpdate(dt);
	}

	virtual void update(float dt) {
		if (world)
			world->update(dt);
	}

	virtual void render(float alpha) {
		if (world)
			world->render(alpha);
	}

	ECS::World* getWorld() { return world.get(); }
	const ECS::World* getWorld() const { return world.get(); }

	SceneManager* getSceneManager() { return sceneManager; }

	/**
	 * @brief Get scene name for debugging.
	 */
	virtual const char* getName() const = 0;
};

} // namespace Blackthorn::Scene