#pragma once

#include <SDL3/SDL.h>

#include "Assets/AssetManager.h"
#include "Core/EngineConfig.h"
#include "Core/Export.h"
#include "Core/Settings.h"
#include "Input/InputManager.h"
#include "Graphics/Renderer.h"
#include "Scene/SceneManager.h"

namespace Blackthorn {

class BLACKTHORN_API Engine {
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	bool init(const EngineConfig& cfg = EngineConfig());
	void shutdown();

	void run();
	bool isRunning() const { return running; }
	void stop() { running = false; }

	void processEvents();
	void render(float alpha);
	void update(float dt);
	void fixedUpdate(float dt);
	void lateUpdate(float dt);

	void logEngineInfo();

	virtual void onRegisterSettings(Core::Settings& settings) {}

	Scene::ISceneContext& getSceneContext() { return *sceneContext; }

private:
	bool initialized = false;
	bool running = false;

	EngineConfig config;
	bool windowFocused = true;

	Assets::AssetManager assetManager;
	std::unique_ptr<Graphics::Renderer> renderer = nullptr;
	Input::InputManager inputManager;
	Scene::SceneManager sceneManager;
	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;

	std::unique_ptr<Scene::ISceneContext> sceneContext = nullptr;

	void initAssetLoaders();
	void cleanupInitialization();

	void registerEngineDefaults(Core::Settings& settings);
	void registerEngineCallbacks(Core::Settings& settings);
	void applyEngineSettings();
	void applyPostProcessing();

	#ifdef BLACKTHORN_DEBUG
		void logProfilingInfo();
		float getFPS() const;
	#endif
};

}