#include "Core/Engine.h"

#include <format>
#include <glad/glad.h>
#include <SDL3_ttf/SDL_ttf.h>

// Loaders
#include "Assets/Loaders/BitmapFontLoader.h"
#include "Assets/Loaders/ShaderLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Loaders/TrueTypeFontLoader.h"

#include "Scene/SceneContext.h"
#include "Debug/Profiler.h"
#include "Debug/Logger.h"

namespace Blackthorn {

Engine::Engine()
	: initialized(false)
	, running(false)
	, windowFocused(true)
	, window(nullptr)
	, glContext(nullptr)
	, sceneContext(nullptr)
{}

Engine::~Engine() {
	shutdown();
}

bool Engine::init(const EngineConfig& cfg) {
	if (initialized) {
		BT_WARN("Engine already initialized.");
		return false;
	}

	config = cfg;
	Debug::Logger::instance().init(cfg.debug.logger);

	SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	if (!SDL_Init(initFlags)) {
		BT_ERROR(std::format("SDL_Init failed: {}", SDL_GetError()));
		cleanupInitialization();
		return false;
	}

	if (!TTF_Init()) {
		BT_ERROR(std::format("TTF_Init failed: {}", SDL_GetError()));
		cleanupInitialization();
		return false;
	}


	#ifdef BLACKTHORN_DEBUG
		BT_LOG("Initializing Blackthorn Engine");
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
		BT_ERROR(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
		cleanupInitialization();
		return false;
	}

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		BT_ERROR(std::format("SDL_GL_CreateContext failed: {}", SDL_GetError()));
		cleanupInitialization();
		return false;
	}

	if (!SDL_GL_MakeCurrent(window, glContext)) {
		BT_ERROR(std::format("SDL_GL_MakeCurrent failed: {}", SDL_GetError()));
		cleanupInitialization();
		return false;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		BT_ERROR("Failed to initialize GLAD");
		cleanupInitialization();
		return false;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (cfg.render.msaaSamples > 0)
		glEnable(GL_MULTISAMPLE);

	if (cfg.window.vsync)
		SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, cfg.window.width, cfg.window.height);

	logEngineInfo();

	try {
		BT_LOG("Initializing Renderer");
		renderer = std::make_unique<Graphics::Renderer>();
		renderer->setClearColor(0.1f, 0.1f, 0.12f);
		renderer->setPostProcessingEnabled(true);
	} catch (const std::exception& e) {
		BT_ERROR(std::format("Failed to initialize Renderer: {}", e.what()));
		cleanupInitialization();
		return false;
	}

	initAssetLoaders();
	int w, h;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	renderer->setProjection(w, h);
	UI::UIManager::onWindowResize(w, h);

	sceneContext = std::make_unique<Scene::SceneContextImpl>(
		assetManager,
		*renderer,
		inputManager,
		sceneManager
	);

	initialized = true;

	BT_LOG("Initialization completed");

	return true;
}

void Engine::initAssetLoaders() {
	assetManager.registerLoader<Graphics::Texture>(
		std::make_unique<Graphics::TextureLoader>(),
		std::make_unique<Graphics::AsyncTextureLoader>()
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

	Fonts::TrueTypeFont::cleanupShader();
	Fonts::BitmapFont::cleanupShader();

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
	Debug::Logger::instance().shutdown();
}

void Engine::render(float alpha) {
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
				int pw, ph;
				SDL_GetWindowSizeInPixels(window, &pw, &ph);

				if (pw != config.window.width || ph != config.window.height) {
					config.window.width = pw;
					config.window.height = ph;

					glViewport(0, 0, pw, ph);
					renderer->setProjection(pw, ph);
					UI::UIManager::onWindowResize(pw, ph);
				}

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
	}

	// #ifdef BLACKTHORN_DEBUG
	// 	if (inputManager.isKeyPressed(SDLK_F5))
	// 		assetManager.reloadAllTyped<Graphics::Texture, Fonts::BitmapFont, Fonts::TrueTypeFont>();
	// #endif
}

void Engine::update(float dt) {
	assetManager.flushPendingUploads(4);

	sceneManager.update(dt);
	inputManager.update(dt);
}

void Engine::fixedUpdate(float dt) {
	sceneManager.fixedUpdate(dt);
}

void Engine::lateUpdate(float dt) {
	sceneManager.lateUpdate(dt);
}

void Engine::run() {
	if (!initialized) {
		BT_ERROR("Cannot run engine: Not initialized");
		return;
	}

	Uint64 lastFrameTime = SDL_GetPerformanceCounter();
	float accumulatedTime = 0.0f;
	const float frequency = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;

	#ifdef BLACKTHORN_DEBUG
		auto& profiler = Debug::Profiler::instance();
	#endif

	while (running) {
		#ifdef BLACKTHORN_DEBUG
			if (windowFocused)
				profiler.beginFrame();

			PROFILE_SCOPE("Frame");
		#endif

		Uint64 currentTime = SDL_GetPerformanceCounter();
		float frameTime = static_cast<float>(currentTime - lastFrameTime) / frequency;
		lastFrameTime = currentTime;

		{
			PROFILE_SCOPE("Events");
			processEvents();
		}

		if (!windowFocused) {
			Uint32 unfocusedDelay = static_cast<Uint32>(1000.0f / config.timing.unfocusedFPS);
			SDL_Delay(unfocusedDelay);

			accumulatedTime = std::min(
				accumulatedTime + frameTime,
				config.timing.fixedDeltaTime * (config.timing.maxFixedUpdates - 1)
			);
			lastFrameTime = SDL_GetPerformanceCounter();
			continue;
		}

		if (frameTime > config.timing.maxDeltaTime) {
			BT_WARN(std::format("Frame time capped: {:.3f} -> {:.3f}", frameTime, config.timing.maxDeltaTime));
			frameTime = config.timing.maxDeltaTime;
		}

		accumulatedTime += frameTime;

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
						BT_WARN(std::format("Too many fixed updates in one frame ({})", fixedUpdateCount));
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

		{
			PROFILE_SCOPE("Late Update");
			lateUpdate(frameTime);
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
				float sleepTime = (targetFrameTime - elapsedTime) - 0.002f;
				if (sleepTime > 0.0f)
					SDL_Delay(static_cast<Uint32>(sleepTime * 1000.0f));

				while (static_cast<float>(SDL_GetPerformanceCounter() - currentTime) / frequency < targetFrameTime);
			}
		}
	}
}

void Engine::logEngineInfo() {
	BT_DEBUG("Engine Info");
	BT_DEBUG(std::format("OpenGL Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION))));
	BT_DEBUG(std::format("Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER))));

	GLint maxTextureSize;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	BT_DEBUG(std::format("Max Texture Size: {}", maxTextureSize));

	GLint maxVertexAttribs;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
	BT_DEBUG(std::format("Max Vertex Attributes: {}", maxVertexAttribs));

	int actualDepthSize, actualStencilSize, actualMSAASamples;

	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &actualDepthSize);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &actualStencilSize);
	SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &actualMSAASamples);

	BT_DEBUG(std::format("Depth Buffer: {} bits (requested {})", actualDepthSize, config.render.depthBits));
	BT_DEBUG(std::format("Stencil Buffer: {} bits (requested {})", actualStencilSize, config.render.stencilBits));
	BT_DEBUG(std::format("MSAA Samples: {}x (requested {}x)", actualMSAASamples, config.render.msaaSamples));

	#if defined(GLM_FORCE_SIMD_AVX2)
		BT_DEBUG("GLM using AVX2 SIMD");
	#elif defined(GLM_FORCE_SIMD_AVX)
		BT_DEBUG("GLM using AVX SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE42)
		BT_DEBUG("GLM using SSE4.2 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE41)
		BT_DEBUG("GLM using SSE4.1 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE3)
		BT_DEBUG("GLM using SSE3 SIMD");
	#elif defined(GLM_FORCE_SIMD_SSE2)
		BT_DEBUG("GLM using SSE2 SIMD");
	#else
		BT_DEBUG("GLM using scalar math (no SIMD)");
	#endif
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

Scene::ISceneContext& Engine::getSceneContext() {
	return *sceneContext;
}

#ifdef BLACKTHORN_DEBUG
	void Engine::logProfilingInfo() {
		auto& profiler = Debug::Profiler::instance();

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
	}

	float Engine::getFPS() const {
		return 1000.0f / Debug::Profiler::instance().getAverageFrameTime(60);
	}
#endif

} // namespace Blackthorn