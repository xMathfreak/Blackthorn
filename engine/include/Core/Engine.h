#pragma once

#include "Engine/Export.h"
#include <string>
#include <SDL3/SDL.h>

namespace Blackthorn {

class BLACKTHORN_API Engine {
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	bool init(const std::string & title = "Blackthorn Engine", int width = 1280, int height = 720);
	void shutdown();
private:
	bool initialized = false;
	bool running = false;

	SDL_Window* window = nullptr;
	SDL_GLContext glContext;
};

}