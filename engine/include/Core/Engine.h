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

	void fixedUpdate(float dt);
	void processEvents();
	void render(float alpha);
	void update(float dt);

	void logEngineInfo();

	Assets::AssetManager& getAssetManager() { return assetManager; }
	Graphics::Renderer* getRenderer() const { return renderer.get(); }
	Input::InputManager& getInputManager() { return inputManager; }
	Scene::SceneManager& getSceneManager() { return sceneManager; }
	SDL_Window* getWindow() const { return window; }

private:
	bool initialized;
	bool running;

	float fps;
	EngineConfig config;
	bool windowFocused;

	Assets::AssetManager assetManager;
	std::unique_ptr<Graphics::Renderer> renderer;
	Input::InputManager inputManager;
	Scene::SceneManager sceneManager;
	SDL_Window* window;
	SDL_GLContext glContext;

	void initAssetLoaders();
	void cleanupInitialization();
};

}