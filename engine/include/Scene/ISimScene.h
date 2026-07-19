#pragma once

#include "Assets/AssetManager.h"
#include "Core/Export.h"
#include "Core/SimClock.h"
#include "Core/Types/Numeric.h"
#include "ECS/World.h"
#include "Jobs/JobSystem.h"
#include "Net/ConnectionManager.h"
#include "Saves/SaveManager.h"
#include "Scene/ISimContext.h"

namespace Blackthorn::Scene {

class SceneManager;

/**
 * @brief Headless base class for a single simulation scene.
 *
 * @details
 * `ISimScene` is the minimal scene type: it owns an ECS `World` and is
 * driven purely by `fixedUpdate()` / `update()` / `lateUpdate()`. It has no
 * knowledge of rendering, audio, or input, which makes it safe to use on
 * both the client and a dedicated server.
 *
 * Client-side scenes that also need rendering, audio, and UI should derive
 * from `IScene` (declared in `Scene/IScene.h`) instead, which extends this
 * class with those capabilities.
 *
 * @par Construction
 * Subclasses do not need to accept or forward a context in their
 * constructor:
 * @code
 * class MyServerScene : public ISimScene {
 * public:
 *     MyServerScene() = default;
 * };
 * @endcode
 * The simulation context is injected automatically by `SceneManager`
 * immediately before `init()` is called - see `SceneManager::pushScene()`
 * and `SceneManager::changeScene()`. Game code never calls `setContext()`
 * directly - it is private and only `SceneManager` is a friend.
 *
 * @par Accessing engine services
 * Common services are available directly as short forwarding methods (e.g.
 * `assets()`, `jobs()`, `simClock()`) so most scene code never needs to
 * reach through `getContext()` at all. `getContext()` remains available
 * for anything not covered by a forwarding method.
 */
class BLACKTHORN_API ISimScene {
	friend class SceneManager;

protected:
	std::unique_ptr<ECS::World> world;

	// Injected automatically by SceneManager via setContext() before init().
	// Never null once init() runs - see setContext().
	ISimContext* context = nullptr;

public:
	ISimScene() = default;
	virtual ~ISimScene() = default;

	virtual void init() {
		world = std::make_unique<ECS::World>(
			ECS::Detail::MAX_ENTITIES,
			&getContext().getJobSystem()
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

	/**
	 * @brief Returns the simulation context injected by `SceneManager`.
	 *
	 * Prefer the short forwarding methods below (`assets()`, `jobs()`, ...)
	 * for common services. Use this directly only for less common access,
	 * or when writing generic code that needs the `ISimContext&` itself.
	 *
	 * @warning Only valid to call from `init()` onward. The context is not
	 * yet set during construction.
	 */
	ISimContext& getContext() { return *context; }
	const ISimContext& getContext() const { return *context; }

	/** @brief Shortcut for `getContext().getAssetManager()`. */
	Assets::AssetManager& assets() { return getContext().getAssetManager(); }

	/** @brief Shortcut for `getContext().getSceneManager()`. */
	SceneManager& sceneManager() { return getContext().getSceneManager(); }

	/** @brief Shortcut for `getContext().getJobSystem()`. */
	Jobs::JobSystem& jobs() { return getContext().getJobSystem(); }

	/** @brief Shortcut for `getContext().getSimClock()`. */
	Core::SimClock& simClock() { return getContext().getSimClock(); }

	/** @brief Shortcut for `getContext().getConnectionManager()`. */
	Net::ConnectionManager& connection() { return getContext().getConnectionManager(); }

	/** @brief Shortcut for `getContext().getSaveManager()`. */
	Saves::SaveManager& saves() { return getContext().getSaveManager(); }

private:
	/**
	 * @brief Injects the simulation context. Called automatically by
	 * `SceneManager` immediately before `init()`. Not callable by game code.
	 */
	void setContext(ISimContext& ctx) { context = &ctx; }
};

} // namespace Blackthorn::Scene
