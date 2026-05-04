#pragma once

#include <atomic>
#include <csignal>
#include <memory>

#include "Assets/AssetManager.h"
#include "Core/EngineConfig.h"
#include "Core/Export.h"
#include "Core/Settings.h"
#include "Core/SimClock.h"
#include "Jobs/JobSystem.h"
#include "Net/ConnectionManager.h"
#include "Saves/SaveManager.h"
#include "Scene/ISimContext.h"
#include "Scene/SceneManager.h"

namespace Blackthorn {

/**
 * @brief Simulation-only engine core shared by both client and server.
 *
 * @details
 * Owns and drives all non-graphics engine systems, including:
 * @li Settings (INI persistence)
 * @li Logger
 * @li SimClock (tick counter, persistence)
 * @li AssetManager (synchronous and asynchronous loading)
 * @li JobSystem (worker thread pool)
 * @li SceneManager (scene stack)
 *
 * Initializes only `SDL_INIT_EVENTS`. No video,
 * rendering, or windowing systems are created, making this class safe
 * for use in dedicated server binaries and headless environments.
 *
 * @section subclassing Subclassing
 * `Engine` (the client build) derives from `EngineCore` and extends it
 * with rendering and presentation systems. Server-side code may either
 * derive from or directly embed `EngineCore`.
 *
 * @section signals Signal Handling
 * `EngineCore::run()` installs `SIGINT` and `SIGTERM` handlers. These
 * handlers set a static atomic flag, causing the main loop to exit
 * cleanly at the end of the current simulation tick.
 *
 * @section lifecycle Lifecycle
 * Typical usage:
 * @code
 * EngineCore engine;
 * engine.init(cfg);
 * engine.getSceneManager().pushScene(...);
 * engine.run();         // blocks until stop() or signal
 * engine.shutdown();    // called automatically by destructor
 * @endcode
 */
class BLACKTHORN_API EngineCore {
public:
	EngineCore();
	virtual ~EngineCore();

	EngineCore(const EngineCore&) = delete;
	EngineCore& operator=(const EngineCore&) = delete;

	/**
	 * @brief Initializes simulation systems.
	 *
	 * Starts the logger, loads settings, initializes SDL events+timer,
	 * creates the job system and asset manager, and registers default asset
	 * loaders. Does not touch SDL video, OpenGL, or any windowing system.
	 *
	 * @param cfg Engine configuration. `cfg.window` and `cfg.render` fields
	 *            are ignored by `EngineCore` - they are only consumed by
	 *            the `Engine` subclass.
	 * @return true on success, false if any critical system failed to initialize.
	 */
	virtual bool init(const EngineConfig& cfg = EngineConfig());

	/**
	 * @brief Shuts down all simulation systems and saves persistent state.
	 *
	 * Safe to call multiple times - subsequent calls are no-ops.
	 * Called automatically by the destructor.
	 */
	virtual void shutdown();

	/**
	 * @brief Runs the main simulation loop until termination.
	 *
	 * @details
	 * Executes a fixed-timestep simulation loop until `stop()` is called or
	 * an external shutdown signal is received. This loop is headless-safe and
	 * contains no rendering; graphical clients extend this behavior in a
	 * derived `run()` implementation.
	 *
	 * Each iteration performs the following steps in order:
	 * @li Process SDL events (keyboard, quit, custom events).
	 * @li Accumulate elapsed frame time against `fixedDeltaTime`.
	 * @li Execute zero or more `fixedUpdate()` steps (simulation tick + dispatch).
	 * @li Execute one `update()` step (variable timestep).
	 * @li Execute one `lateUpdate()` step.
	 * @li Flush the main-thread job queue.
	 * @li Sleep for any remaining frame budget if `frame_cap` is enabled.
	 *
	 * @note
	 * Rendering is intentionally excluded from this loop. The client-side
	 * `Engine` class extends this behavior by adding a render step in its
	 * own `run()` implementation.
	 */
	virtual void run();

	/** @brief Requests the loop to exit after the current tick completes. */
	void stop() { running = false; }

	bool isRunning() const { return running; }
	bool isInitialized() const { return initialized; }

	Scene::ISimContext& getSimContext() { return *simContext; }
	Scene::SceneManager& getSceneManager() { return *sceneManager; }
	Assets::AssetManager& getAssetManager() { return *assetManager; }
	Jobs::JobSystem& getJobSystem() { return *jobSystem; }
	Core::SimClock& getSimClock() { return *simClock; }
	const Core::SimClock& getSimClock() const { return *simClock; }
	Net::ConnectionManager& getConnectionManager() { return *connectionManager; }
	Saves::SaveManager& getSaveManager() { return *saveManager; }

	/**
	 * @brief Called during `init()` after engine defaults are registered
	 * but before settings are saved.
	 *
	 * Override to register application-specific settings defaults or to
	 * add `Settings::onChange` callbacks.
	 */
	virtual void onRegisterSettings(Core::Settings&) {}

	/**
	 * @brief Called during @c init after built in save sections have been
	 * registered but before the manager is exposed to scenes.
	 *
	 * Override to register game-specific @c ISaveSection implementations.
	 * At this point, @c ClockSaveSection and @c MetaSaveSection are already
	 * registered. @c WorldSaveSection must be registered here or by scene code
	 * because it requires an @c EntityPool and @c SaveEntityRegistry.
	 *
	 * @param saves Reference to the engine-owned @c SaveManager.
	 */
	virtual void onRegisterSaveSections(Saves::SaveManager& saves) {}

	/**
	 * @brief Returns the @c SaveId used for the automatic shutdown save.
	 *
	 * The default implementation derives a stable, deterministic @c UUID from
	 * the fixed string @c "blackthorn.autosave.shutdown" using FNV-1a, ensuring
	 * the storage backend always resolves to the same file path and overwrites
	 * the previous shutdown save rather than accumulating new files.
	 *
	 * Override to scope the save per-world or per-player:
	 * @code
	 * Saves::SaveId MyGame::getShutdownSaveId() const override {
	 *     Saves::SaveId id = EngineCore::getShutdownSaveId();
	 *     id.worldId  = currentWorldId;
	 *     id.playerId = currentPlayerId;
	 *     return id;
	 * }
	 * @endcode
	 *
	 * @note The returned @c SaveId::id (UUID) determines the file path via the
	 * storage backend's path resolver. Changing it means a different file.
	 * Override the UUID only if you intentionally want separate files.
	 */
	virtual Saves::SaveId getShutdownSaveId() const;

protected:
	bool initialized = false;
	bool running = false;

	EngineConfig config;

	std::unique_ptr<Assets::AssetManager> assetManager;
	std::unique_ptr<Net::ConnectionManager> connectionManager;
	std::unique_ptr<Jobs::JobSystem> jobSystem;
	std::unique_ptr<Saves::SaveManager> saveManager;
	std::unique_ptr<Core::SimClock> simClock;
	std::unique_ptr<Scene::ISimContext> simContext;
	std::unique_ptr<Scene::SceneManager> sceneManager;

	virtual void processEvents();
	virtual void fixedUpdate(float dt);
	virtual void update(float dt);
	virtual void lateUpdate(float dt);

	virtual void registerEngineCallbacks(Core::Settings& s);
	virtual void registerDefaultSettings(Core::Settings& s);
	void applyCoreSettings();
	void cleanupInitialization();

	/**
	 * @brief Installs SIGINT and SIGTERM handlers that set `signalReceived`.
	 * Called once at the start of `run()`.
	 */
	static void installSignalHandlers();
	static void signalHandler(int);

	/// Set to true by the signal handler. Checked at the top of each loop
	/// iteration in run(). Using a signal-safe atomic type.
	static inline std::atomic<bool> signalReceived { false };

	#ifdef BLACKTHORN_DEBUG
		void logProfilingInfo();
	#endif

private:
	/**
	 * @brief Constructs the SaveManager from config, registers built-in
	 * sections, then calls @c onRegisterSaveSections for the game layer.
	 */
	void initSaveManager();
};

} // namespace Blackthorn