#pragma once

#include <SDL3/SDL.h>

#include "Assets/AssetManager.h"
#include "Core/EngineConfig.h"
#include "Core/Export.h"
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

	Scene::ISceneContext& getSceneContext();

private:
	bool initialized;
	bool running;

	EngineConfig config;
	bool windowFocused;

	Assets::AssetManager assetManager;
	std::unique_ptr<Graphics::Renderer> renderer;
	Input::InputManager inputManager;
	Scene::SceneManager sceneManager;
	SDL_Window* window;
	SDL_GLContext glContext;

	std::unique_ptr<Scene::ISceneContext> sceneContext;

	void initAssetLoaders();
	void cleanupInitialization();

	#ifdef BLACKTHORN_DEBUG
		void logProfilingInfo();
		float getFPS() const;
	#endif
};

}