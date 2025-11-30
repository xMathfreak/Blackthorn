#pragma once

#include "Core/EngineConfig.h"
#include "Core/Export.h"

#include <SDL3/SDL.h>

namespace Blackthorn {

class BLACKTHORN_API Engine {
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	bool init(const EngineConfig& config = EngineConfig());
	void shutdown();

	void run();
	bool isRunning() const { return running; }
	void stop() { running = false; }

	void fixedUpdate(float dt);
	void processEvents();
	void render(float alpha);
	void update(float dt);
private:
	bool initialized;
	bool running;

	EngineConfig config;

	SDL_Window* window;
	SDL_GLContext glContext;
};

}