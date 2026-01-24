#include "Core/Engine.h"

#include <glad/glad.h>
#include <SDL3_ttf/SDL_ttf.h>

// Loaders
#include "Assets/Loaders/BitmapFontLoader.h"
#include "Assets/Loaders/ShaderLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Loaders/TrueTypeFontLoader.h"

#include "Debug/Profiler.h"

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

	if (!TTF_Init()) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "TTF_Init failed: %s", SDL_GetError());
		return false;
	}
	
	config = cfg;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("========= Initializing Blackthorn Engine ==========");
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
		TTF_Quit();
		SDL_Quit();
		return false;
	}

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_GL_CreateContext failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		TTF_Quit();
		SDL_Quit();
		return false;
	}

	if (!SDL_GL_MakeCurrent(window, glContext)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		cleanupInitialization();
		return false;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize GLAD");
		cleanupInitialization();
		return false;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (cfg.render.msaaSamples > 0)
		glEnable(GL_MULTISAMPLE);

	if (cfg.window.vsync)
		SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, cfg.window.width, cfg.window.height);

	#ifdef BLACKTHORN_DEBUG
		logEngineInfo();
	#endif

	try {
		#ifdef BLACKTHORN_DEBUG
			SDL_Log("============== Initializing Renderer ==============");
		#endif

		renderer = std::make_unique<Graphics::Renderer>();

		#ifdef BLACKTHORN_DEBUG
			SDL_Log("===================================================");
		#endif
	} catch (const std::exception& e) {
		SDL_LogError(
			SDL_LOG_CATEGORY_RENDER,
			"Failed to initialize Renderer: %s",
			e.what()
		);

		cleanupInitialization();

		return false;
	}

	initAssetLoaders();
	initialized = true;

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("=== Blackthorn Engine initialization successful ===");
	#endif

	return true;
}

void Engine::initAssetLoaders() {
	assetManager.registerLoader<Graphics::Texture>(
		std::make_unique<Graphics::TextureLoader>()
	);

	assetManager.registerLoader<Graphics::Shader>(
		std::make_unique<Graphics::ShaderLoader>()
	);

	assetManager.registerLoader<Fonts::BitmapFont>(
		std::make_unique<Fonts::BitmapFontLoader>()
	);

	assetManager.registerLoader<Fonts::TrueTypeFont>(
		std::make_unique<Fonts::TrueTypeFontLoader>()
	);
}

void Engine::shutdown() {
	if (!initialized)
		return;

	assetManager.clear();

	if (glContext) {
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	TTF_Quit();
	SDL_Quit();

	initialized = false;
	running = false;
}

void Engine::render(float alpha) {
	glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer->beginScene();
	
	sceneManager.render(alpha);

	renderer->endScene();

	SDL_GL_SwapWindow(window);
}

void Engine::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		inputManager.handleEvent(event);

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
			default:
				break;
		}

		#ifdef BLACKTHORN_DEBUG
			if (inputManager.isKeyPressed(SDLK_F5))
				assetManager.reloadAllTyped<Graphics::Texture, Fonts::BitmapFont, Fonts::TrueTypeFont>();
		#endif
	}
}

void Engine::update(float dt) {
	inputManager.update(dt);
	sceneManager.update(dt);
}

void Engine::fixedUpdate(float dt) {
	sceneManager.fixedUpdate(dt);
}

void Engine::run() {
	if (!initialized) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Cannot run engine: Not initialized");
		return;
	}

	PROFILE_SCOPE("Frame");

	Uint64 lastFrameTime = SDL_GetPerformanceCounter();
	float accumulatedTime = 0.0f;
	const float frequency = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;

	#ifdef BLACKTHORN_DEBUG
		auto& profiler = Debug::Profiler::instance();
	#endif

	while (running) {
		#ifdef BLACKTHORN_DEBUG
			profiler.beginFrame();
		#endif

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

		{
			PROFILE_SCOPE("Events");
			processEvents();
		}

		if (!windowFocused) {
			#ifdef BLACKTHORN_DEBUG
				profiler.endFrame();
			#endif
			
			Uint32 unfocusedDelay = static_cast<Uint32>(1000.0f / config.timing.unfocusedFPS);
			SDL_Delay(unfocusedDelay);
			continue;
		}

		int fixedUpdateCount = 0;

		{
			PROFILE_SCOPE("Fixed Update Loop");
			while (accumulatedTime >= config.timing.fixedDeltaTime) {
				PROFILE_SCOPE("Fixed Update");
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
		}

		{
			PROFILE_SCOPE("Update");
			update(frameTime);
		}

		float alpha = accumulatedTime / config.timing.fixedDeltaTime;
		{
			PROFILE_SCOPE("Render");
			render(alpha);
		}

		#ifdef BLACKTHORN_DEBUG
			static float logCounter = 0.0f;
			logCounter += frameTime;
			
			profiler.endFrame();
			
			if (logCounter >= config.debug.profilingLogInterval) {
				logProfilingInfo();
				logCounter = 0;
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
	SDL_Log("=================== Engine Info ===================");
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

	SDL_Log("===================================================");
}

void Engine::cleanupInitialization() {
	if (glContext) {
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	TTF_Quit();
	SDL_Quit();
}

#ifdef BLACKTHORN_DEBUG
	void Engine::logProfilingInfo() {
		auto& profiler = Debug::Profiler::instance();

		SDL_Log("====== Performance Stats (60 frames average) ======");
		SDL_Log("Frame Time: %.2f ms (%.1f FPS)",
			profiler.getAverageFrameTime(60),
			1000.0f / profiler.getAverageFrameTime(60)
		);

		auto scopeNames = profiler.getAllScopeNames();
		for (const auto& name : scopeNames) {
			auto stats = profiler.getStats(name, 60);

			if (stats.average > 0.1f) {
				SDL_Log(" %s: %.2f ms (min: %.2f, max: %.2f, calls: %d)",
					name.c_str(),
					stats.average,
					stats.min,
					stats.max,
					stats.callCount
				);
			}
		}

		SDL_Log("===================================================");
	}

	float Engine::getFPS() const {
		return 1000.0f / Debug::Profiler::instance().getAverageFrameTime(60);
	}
#endif

} // namespace Blackthorn