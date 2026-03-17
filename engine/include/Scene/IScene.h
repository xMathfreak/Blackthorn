#pragma once

#include "Core/Export.h"
#include "ECS/World.h"
#include "Scene/ISceneContext.h"
#include "UI/UIManager.h"

namespace Blackthorn::Scene {

class BLACKTHORN_API IScene {
protected:
	std::unique_ptr<ECS::World> world;
	std::unique_ptr<UI::UIManager> uiManager;

	ISceneContext& context;

public:
	IScene(ISceneContext& ctx)
		: context(ctx)
	{}

	virtual ~IScene() = default;

	virtual void init() {
		world = std::make_unique<ECS::World>();
		uiManager = std::make_unique<UI::UIManager>();
	}

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

	virtual void lateUpdate(float dt) {
		if (world)
			world->lateUpdate(dt);
	}

	ECS::World* getWorld() { return world.get(); }
	const ECS::World* getWorld() const { return world.get(); }

	/**
	 * @brief Get scene name for debugging.
	 */
	virtual const char* getName() const = 0;
};

} // namespace Blackthorn::Scene