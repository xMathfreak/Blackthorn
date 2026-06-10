#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "ECS/World.h"
#include "Scene/ISimContext.h"

namespace Blackthorn::Scene {

class BLACKTHORN_API IScene {
protected:
	std::unique_ptr<ECS::World> world;

	// Initialize this in constructor using std::forward + ctor args
	ISimContext& context;

public:
	explicit IScene(ISimContext& ctx)
		: context(ctx)
	{}

	virtual ~IScene() = default;

	virtual void init() {
		world = std::make_unique<ECS::World>(
			ECS::Detail::MAX_ENTITIES,
			&context.getJobSystem()
		);
	}

	virtual void onEnter() {}
	virtual void onExit() {}
	virtual void onPause() {}
	virtual void onResume() {}

	virtual bool blocksUpdate() const { return true; }
	virtual bool blocksRender() const { return true; }

	virtual void fixedUpdate(float dt, U64 tick) {
		if (world)
			world->fixedUpdate(dt, tick);
	}

	virtual void update(float dt) {
		if (world)
			world->update(dt);
	}

	virtual void lateUpdate(float dt) {
		if (world)
			world->lateUpdate(dt);
	}

	ECS::World* getWorld() { return world.get(); }
	const ECS::World* getWorld() const { return world.get(); }

	/** @brief Human-readable scene name used for logging and debugging. */
	virtual const char* getName() const { return ""; }
};

} // namespace Blackthorn::Scene