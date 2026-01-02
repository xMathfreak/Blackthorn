#pragma once

#include "Core/EngineConfig.h"
#include "Core/Export.h"
#include "Graphics/Renderer.h"
#include "ECS/World.h"

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

	SDL_Window* getWindow() const { return window; }
	Graphics::Renderer* getRenderer() const { return renderer.get(); }
	ECS::World& getWorld() { return world; }
private:
	bool initialized;
	bool running;

	bool windowFocused;

	float fps;

	EngineConfig config;

	SDL_Window* window;
	SDL_GLContext glContext;
	std::unique_ptr<Graphics::Renderer> renderer;
	ECS::World world;

	void initDefaultSystems();
};

}