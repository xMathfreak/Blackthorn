#include "Core/Engine.h"

#include <glad/glad.h>

namespace Blackthorn {

Engine::Engine()
	: initialized(false)
	, running(false)
	, windowFocused(true)
	, window(nullptr)
	, glContext(nullptr)
{}

Engine::~Engine() {
	shutdown();
}

bool Engine::init(const EngineConfig& cfg) {
	if (initialized) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Engine already initialized.");
		return false;
	}

	SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	if (!SDL_Init(initFlags)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s", SDL_GetError());
		return false;
	}
	
	config = cfg;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Initializing Blackthorn Engine");
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
	#else
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
	#endif

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, cfg.render.openglMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, cfg.render.openglMinor);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, cfg.render.depthBits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, cfg.render.stencilBits);

	if (cfg.render.msaaSamples > 0) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, cfg.render.msaaSamples);
	}

	SDL_WindowFlags windowFlags = SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	if (cfg.window.fullscreen)
		windowFlags |= SDL_WINDOW_FULLSCREEN;

	window = SDL_CreateWindow(cfg.window.title.c_str(), cfg.window.width, cfg.window.height, windowFlags);
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


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (cfg.render.msaaSamples > 0)
		glEnable(GL_MULTISAMPLE);

	if (cfg.window.vsync)
		SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, cfg.window.width, cfg.window.height);

	try {
		renderer = std::make_unique<Graphics::Renderer>();
	} catch (const std::exception& e) {
		SDL_LogError(
			SDL_LOG_CATEGORY_RENDER,
			"Failed to initialize Renderer: %s",
			e.what()
		);

		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();

		return false;
	}

	#ifdef BLACKTHORN_DEBUG
		logEngineInfo();
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
	glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer->beginScene();

	#ifdef BLACKTHORN_DEBUG
		SDL_FRect testRect = {100.0f, 100.0f, 100.0f, 100.0f};
		Graphics::Texture tex("assets/image.png");
		// SDL_FColor testColor = {1.0f, 0.0f, 1.0f, 1.0f};
		renderer->drawTexture(tex, testRect);
	#endif

	renderer->endScene();

	SDL_GL_SwapWindow(window);
}

void Engine::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				config.window.width = event.window.data1;
				config.window.height = event.window.data2;
				glViewport(0, 0, event.window.data1, event.window.data2);
				renderer->setProjection(event.window.data1, event.window.data2);
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				windowFocused = true;
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				windowFocused = false;
				break;
		}
	}
}

void Engine::update(float dt) {

}

void Engine::fixedUpdate(float dt) {

}

void Engine::run() {
	if (!initialized) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Cannot run engine: Not initialized");
		return;
	}

	Uint64 lastFrameTime = SDL_GetPerformanceCounter();
	float accumulatedTime = 0.0f;
	const float frequency = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;

	#ifdef BLACKTHORN_DEBUG
		Uint64 lastFPSTime = lastFrameTime;
		int frameCount = 0;
		fps = 0;
	#endif

	while (running) {
		Uint64 currentTime = SDL_GetPerformanceCounter();
		float frameTime = static_cast<float>(currentTime - lastFrameTime) / frequency;
		lastFrameTime = currentTime;

		if (frameTime > config.timing.maxDeltaTime) {
			#ifdef BLACKTHORN_DEBUG
				SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					"Frame time capped: %.3f -> %.3f",
					frameTime, config.timing.maxDeltaTime
				);
			#endif

			frameTime = config.timing.maxDeltaTime;
		}

		accumulatedTime += frameTime;

		processEvents();

		if (!windowFocused) {
			Uint32 unfocusedDelay = static_cast<Uint32>(1000.0f / config.timing.unfocusedFPS);
			SDL_Delay(unfocusedDelay);
			continue;
		}

		int fixedUpdateCount = 0;

		while (accumulatedTime >= config.timing.fixedDeltaTime) {
			fixedUpdate(config.timing.fixedDeltaTime);
			accumulatedTime -= config.timing.fixedDeltaTime;
			fixedUpdateCount++;

			if (fixedUpdateCount > config.timing.maxFixedUpdates) {
				#ifdef BLACKTHORN_DEBUG
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
						"Too many fixed updates in one frame (%d)",
						fixedUpdateCount
					);
				#endif

				accumulatedTime = 0.0f;
				break;
			}
		}


		update(frameTime);

		float alpha = accumulatedTime / config.timing.fixedDeltaTime;
		render(alpha);

		#ifdef BLACKTHORN_DEBUG
			frameCount++;
			float elapsedFPSTime = static_cast<float>(currentTime - lastFPSTime) / frequency;
			if (elapsedFPSTime > 1.0f) {
				fps = frameCount / elapsedFPSTime;
				SDL_Log("FPS: %.2f (Frame Time: %.3fms)", fps, (1000.0f / fps));
				frameCount = 0;
				lastFPSTime = currentTime;
			}
		#endif

		if (config.timing.capFrameRate && !config.window.vsync) {
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

void Engine::logEngineInfo() {
	SDL_Log("OpenGL Version: %s", glGetString(GL_VERSION));
	SDL_Log("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
	SDL_Log("Renderer: %s", glGetString(GL_RENDERER));
	SDL_Log("Vendor: %s", glGetString(GL_VENDOR));

	GLint maxTextureSize;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	SDL_Log("Max Texture Size: %d", maxTextureSize);

	GLint maxVertexAttribs;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
	SDL_Log("Max Vertex Attributes: %d", maxVertexAttribs);
	int actualDepthSize, actualStencilSize, actualMSAASamples;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &actualDepthSize);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &actualStencilSize);
	SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &actualMSAASamples);
	SDL_Log("Depth Buffer: %d bits (requested %d)", actualDepthSize, config.render.depthBits);
	SDL_Log("Stencil Buffer: %d bits (requested %d)", actualStencilSize, config.render.stencilBits);
	SDL_Log("MSAA Samples: %dx (requested %dx)", actualMSAASamples, config.render.msaaSamples);

	#if defined(GLM_FORCE_SIMD_AVX2)
		SDL_Log("GLM using AVX2 SIMD");
	#elif defined(GLM_FORCE_SIMD_AVX)
		SDL_Log("GLM using AVX SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE42)
		SDL_Log("GLM using SSE4.2 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE41)
		SDL_Log("GLM using SSE4.1 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE3)
		SDL_Log("GLM using SSE3 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE2)
		SDL_Log("GLM using SSE2 SIMD");
	#else
		SDL_Log("GLM using scalar math (no SIMD)");
	#endif
}

}