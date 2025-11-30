#include "Core/Engine.h"

#include <glad/glad.h>

namespace Blackthorn {

Engine::Engine()
	: initialized(false)
	, running(false)
	, glContext(nullptr)
	, window(nullptr)
{}

Engine::~Engine() {
	shutdown();
}

bool Engine::init(const EngineConfig& config) {
	if (initialized) {
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.render.openglMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.render.openglMinor);

	SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	if (!SDL_Init(initFlags)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	SDL_WindowFlags windowFlags = SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	window = SDL_CreateWindow(config.window.title.c_str(), config.window.width, config.window.height, windowFlags);
	if (!window) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return false;
	}

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_GL_CreateContext failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return false;
	}

	if (!SDL_GL_MakeCurrent(window, glContext)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return false;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize GLAD");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return false;
	}

	#ifdef DEBUG
	SDL_Log("OpenGL Version: %s", glGetString(GL_VERSION));
	SDL_Log("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
	SDL_Log("Renderer: %s", glGetString(GL_RENDERER));
	#endif

	initialized = true;
	return true;
}

void Engine::shutdown() {
	if (!initialized)
		return;

	if (glContext) {
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	SDL_Quit();
	initialized = false;
	running = false;
}

void Engine::render(float alpha) {

}

void Engine::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
		}
	}
}

void Engine::update(float dt) {

}

void Engine::fixedUpdate(float dt) {

}

void Engine::run() {
	Uint64 lastFrameTime = SDL_GetPerformanceCounter();
	float accumulatedTime = 0.0f;
	const float frequency = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;

	while (running) {
		Uint64 currentTime = SDL_GetPerformanceCounter();
		float frameTime = static_cast<float>(currentTime - lastFrameTime) / frequency;
		lastFrameTime = currentTime;

		if (frameTime > config.timing.maxDeltaTime)
			frameTime = config.timing.maxDeltaTime;

		accumulatedTime += frameTime;

		processEvents();

		while (accumulatedTime >= config.timing.fixedDeltaTime) {
			fixedUpdate(config.timing.fixedDeltaTime);
			accumulatedTime -= config.timing.fixedDeltaTime;
		}

		update(frameTime);

		float alpha = accumulatedTime / config.timing.fixedDeltaTime;
		render(alpha);

		if (config.timing.capFrameRate) {
			float targetFrameTime = 1.0f / config.timing.targetFPS;
			Uint64 endTime = SDL_GetPerformanceCounter();
			float elapsedTime = static_cast<float>(endTime - currentTime) / frequency;

			if (elapsedTime < targetFrameTime) {
				Uint32 delay = static_cast<Uint32>((targetFrameTime - elapsedTime) * 1000.0f);
				SDL_Delay(delay);
			}
		}
	}
}

}