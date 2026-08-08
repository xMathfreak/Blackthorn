#pragma once

#include <SDL3/SDL.h>

#include "Audio/AudioManager.h"
#include "Core/EngineCore.h"
#include "Core/Export.h"
#include "Graphics/Renderer.h"
#include "Input/InputManager.h"
#include "Scene/ClientSceneManager.h"
#include "Scene/ISceneContext.h"

namespace Blackthorn {

/**
 * @brief Graphics-enabled engine implementation for the client build.
 *
 * @details
 * Extends `EngineCore` with rendering and presentation capabilities.
 * In addition to the core simulation systems, this class initializes
 * SDL video, creates an OpenGL context, and owns the `Renderer`.
 *
 * Overrides `run()` to include a render step and interpolation alpha,
 * enabling smooth visual updates between fixed simulation ticks.
 *
 * @section usage Usage
 * The dedicated server links against `BlackthornCore` and uses
 * `EngineCore` directly. Client applications link against
 * `BlackthornEngine` and use this class.
 *
 * @note
 * Do not define `BLACKTHORN_HEADLESS` when compiling this target, as
 * rendering and windowing functionality are required.
 */
class BLACKTHORN_API Engine : public EngineCore {
public:
	Engine() = default;
	~Engine() override;

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	/**
	 * @brief Initializes simulation systems (via EngineCore) then graphics.
	 *
	 * Call order:
	 *   1. `EngineCore::init()` - settings, logger, SDL events+timer,
	 *      asset manager, job system.
	 *   2. SDL video, OpenGL context creation, GLAD loading.
	 *   3. Renderer construction, FBO, screen shader.
	 *   4. Replaces `simContext` with a `SceneContextImpl` that also
	 *      exposes the renderer via `ISceneContext`.
	 *
	 * @param cfg Engine configuration. All fields are consumed here,
	 *            including `cfg.window` and `cfg.render`.
	 */
	bool init(const EngineConfig& cfg = EngineConfig()) override;

	/**
	 * @brief Shuts down graphics resources then delegates to EngineCore.
	 */
	void shutdown() override;

	/**
	 * @brief Runs the client loop (simulation + render).
	 *
	 * Follows the same fixed-timestep structure as `EngineCore::run()`
	 * (events, fixed updates, update, late update) but adds a render step
	 * after `lateUpdate()` using the interpolation alpha computed from the
	 * accumulated fixed-update remainder.
	 *
	 * The frame-cap section also differs from `EngineCore::run()`: this
	 * override additionally skips capping when vsync is enabled, since
	 * vsync already throttles the loop via the buffer swap. `EngineCore`
	 * has no window/vsync concept, so its own frame cap has no such gate.
	 */
	void run() override;

	/**
	 * @brief Returns the full scene context, including renderer access.
	 */
	Scene::ISceneContext& getSceneContext() {
		return static_cast<Scene::ISceneContext&>(*simContext);
	}

	Scene::ClientSceneManager& getClientSceneManager() {
		return static_cast<Scene::ClientSceneManager&>(*sceneManager);
	}

	void update(float dt) override;

	Audio::AudioManager& getAudioManager() { return *audioManager; }
	Input::InputManager& getInputManager() { return inputManager; }
	Graphics::Renderer& getRenderer() { return *renderer; }

protected:
	void render(float alpha);
	void processEvents() override;
	void applyEngineSettings();
	void applyPostProcessing();
	void registerEngineCallbacks(Core::Settings& settings) override;
	void registerDefaultSettings(Core::Settings& s) override;

private:
	std::unique_ptr<Graphics::Renderer> renderer;
	std::unique_ptr<Audio::AudioManager> audioManager;
	Input::InputManager inputManager;
	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;

	/**
	 * @brief Cached mirror of the "window/vsync" setting.
	 */
	Core::CachedSetting<bool> vsyncEnabled{"window", "vsync", true};

	/**
	 * @brief Cached mirror of the "window/unfocused_fps" setting.
	 */
	Core::CachedSetting<U32> unfocusedFPS{"window", "unfocused_fps", 0};

	/**
	 * @brief Cached mirror of the "window/minimized_fps" setting.
	 */
	Core::CachedSetting<U32> minimizedFPS{"window", "minimized_fps", 0};

	bool windowFocused = false;
	bool windowMinimized = true;

	void initGraphics(const EngineConfig& cfg);
	void initAssetLoaders();
	void cleanupGraphics();
	void logEngineInfo();
};

}