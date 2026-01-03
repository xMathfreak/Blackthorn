#pragma once

#include "Assets/AssetManager.h"
#include "Core/EngineConfig.h"
#include "Core/Export.h"
#include "ECS/World.h"
#include "Graphics/Renderer.h"

#include <SDL3/SDL.h>

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
	ECS::World& getWorld() { return world; }
	SDL_Window* getWindow() const { return window; }
	Graphics::Renderer* getRenderer() const { return renderer.get(); }
private:
	bool initialized;
	bool running;

	bool windowFocused;

	float fps;

	EngineConfig config;

	Assets::AssetManager assetManager;
	ECS::World world;
	SDL_Window* window;
	SDL_GLContext glContext;
	std::unique_ptr<Graphics::Renderer> renderer;

	void initDefaultSystems();
	void initAssetLoaders();
};

}