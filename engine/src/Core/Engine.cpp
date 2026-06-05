#include "Core/Engine.h"

#include <SDL3_ttf/SDL_ttf.h>

#include "Assets/Loaders/AudioLoader.h"
#include "Assets/Loaders/BitmapFontLoader.h"
#include "Assets/Loaders/ShaderLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Loaders/TrueTypeFontLoader.h"

#include "Core/Settings.h"
#include "Debug/Logger.h"
#include "Debug/Profiler.h"
#include "Fonts/BitmapFont.h"
#include "Fonts/TrueTypeFont.h"
#include "Scene/SceneContext.h"
#include "UI/UIManager.h"

namespace Blackthorn {

namespace {

inline std::string getSIMDInfo() {
	std::string simd = "scalar"; // default

#if defined(GLM_FORCE_INTRINSICS)
	// GCC / Clang
	#if defined(__AVX512F__)
		simd = "AVX-512";
	#elif defined(__AVX2__)
		simd = "AVX2";
	#elif defined(__AVX__)
		simd = "AVX";
	#elif defined(__SSE4_2__)
		simd = "SSE4.2";
	#elif defined(__SSE4_1__)
		simd = "SSE4.1";
	#elif defined(__SSSE3__)
		simd = "SSSE3";
	#elif defined(__SSE3__)
		simd = "SSE3";
	#elif defined(__SSE2__)
		simd = "SSE2";
	#else
		simd = "unknown SIMD";
	#endif

	// MSVC
	#ifdef _MSC_VER
		#if defined(__AVX512F__)
			simd = "AVX-512";
		#elif defined(__AVX2__)
			simd = "AVX2";
		#elif defined(__AVX__)
			simd = "AVX";
		#elif defined(__SSE4_2__)
			simd = "SSE4.2";
		#elif defined(__SSE4_1__)
			simd = "SSE4.1";
		#elif defined(__SSSE3__)
			simd = "SSSE3";
		#elif defined(__SSE3__)
			simd = "SSE3";
		#elif defined(__SSE2__) || defined(_M_IX86_FP) && _M_IX86_FP >= 2
			simd = "SSE2";
		#endif
	#endif

#endif // GLM_FORCE_INTRINSICS

	return simd;
}

}

Engine::~Engine() {
	shutdown();
}

bool Engine::init(const EngineConfig& cfg) {
	if (!EngineCore::init(cfg))
		return false;

	sceneManager = std::make_unique<Scene::ClientSceneManager>();

	initGraphics(cfg);
	if (!window || !glContext) {
		EngineCore::shutdown();
		return false;
	}

	audioManager = std::make_unique<Audio::AudioManager>();

	initAssetLoaders();

	applyEngineSettings();

	if (!audioManager->init(cfg.audio)) {
		EngineCore::shutdown();
		return false;
	}

	try {
		BT_LOG("Renderer: Initializing");
		renderer = std::make_unique<Graphics::Renderer>(cfg.render.maxQuads);
		renderer->setPostProcessingEnabled(
			Core::Settings::instance().get<bool>("graphics", "post_processing")
		);
	} catch (const std::exception& e) {
		BT_ERROR("Renderer: Failed to initialize - {}", e.what());
		cleanupGraphics();
		EngineCore::shutdown();
		return false;
	}

	int w, h;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	renderer->setProjection(w, h);
	UI::UIManager::onWindowResize(w, h);

	simContext = std::make_unique<Scene::SceneContextImpl>(
		*audioManager,
		*assetManager,
		*connectionManager,
		inputManager,
		*jobSystem,
		*sceneManager,
		*simClock,
		*renderer,
		*saveManager
	);

	applyPostProcessing();
	logEngineInfo();

	BT_LOG("Engine: Initialization complete [Runtime]");
	return true;
}

void Engine::initGraphics(const EngineConfig& cfg) {
	#ifdef BLACKTHORN_DEBUG
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
	#else
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
	#endif

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		BT_ERROR("SDL: Failed to initialize Video subsystem - {}", SDL_GetError());
		return;
	}

	if (!TTF_Init()) {
		BT_ERROR("SDL_ttf: Failed to initialize font subsystem - {}", SDL_GetError());
		return;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, cfg.render.openglMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, cfg.render.openglMinor);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, cfg.render.depthBits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, cfg.render.stencilBits);

	auto& settings = Core::Settings::instance();

	int msaaSamples = settings.get<int>("graphics", "msaa_samples");
	if (msaaSamples > 0) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaaSamples);
	}

	window = SDL_CreateWindow(
		cfg.window.title.c_str(),
		cfg.window.width, cfg.window.height,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
	);

	if (!window) {
		BT_ERROR("Window: Failed to create SDL window - {}", SDL_GetError());
		return;
	}

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		BT_ERROR("Renderer (OpenGL): Failed to create GL context - {}", SDL_GetError());
		return;
	}

	if (!SDL_GL_MakeCurrent(window, glContext)) {
		BT_ERROR("Renderer (OpenGL): Failed to make context current - {}", SDL_GetError());
		return;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		BT_ERROR("Renderer (OpenGL): Failed to initialize GLAD");
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (msaaSamples > 0)
		glEnable(GL_MULTISAMPLE);

	glViewport(0, 0, cfg.window.width, cfg.window.height);
}

void Engine::shutdown() {
	if (!initialized)
		return;

	Fonts::TrueTypeFont::cleanupShader();
	Fonts::BitmapFont::cleanupShader();

	renderer.reset();
	audioManager.reset();
	cleanupGraphics();

	EngineCore::shutdown();
}

void Engine::cleanupGraphics() {
	if (glContext) {
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	TTF_Quit();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Engine::initAssetLoaders() {
	assetManager->registerLoader<Audio::AudioClip>(
		std::make_unique<Audio::AudioLoader>(),
		std::make_unique<Audio::AsyncAudioLoader>()
	);

	assetManager->registerLoader<Graphics::Texture>(
		std::make_unique<Graphics::TextureLoader>(),
		std::make_unique<Graphics::AsyncTextureLoader>()
	);

	assetManager->registerLoader<Graphics::Shader>(
		std::make_unique<Graphics::ShaderLoader>()
	);

	assetManager->registerLoader<Fonts::BitmapFont>(
		std::make_unique<Fonts::BitmapFontLoader>()
	);

	assetManager->registerLoader<Fonts::TrueTypeFont>(
		std::make_unique<Fonts::TrueTypeFontLoader>()
	);
}

void Engine::run() {
	if (!initialized) {
		BT_ERROR("Cannot run: Engine not initialized");
		return;
	}

	installSignalHandlers();

	U64 lastTime = SDL_GetPerformanceCounter();
	float accumulated = 0.0f;
	const float freq = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;
	auto& settings = Core::Settings::instance();

	#ifdef BLACKTHORN_DEBUG
		auto& profiler = Debug::Profiler::instance();
	#endif

	while (running && !signalReceived.load(std::memory_order::relaxed)) {
		#ifdef BLACKTHORN_DEBUG
			profiler.beginFrame();
			PROFILE_SCOPE("Frame");
		#endif

		const U64 now = SDL_GetPerformanceCounter();
		float frameTime = static_cast<float>(now - lastTime) / freq;
		lastTime = now;

		{
			PROFILE_SCOPE("Events");
			processEvents();
		}

		if (frameTime > config.timing.maxDeltaTime) {
			BT_WARN(
				"Timing: Frame time capped {:.3f} -> {:.3f}",
				frameTime, config.timing.maxDeltaTime
			);

			frameTime = config.timing.maxDeltaTime;
		}

		accumulated += frameTime;

		int numFixed = 0;
		float accumulatedCopy = accumulated;

		while (accumulatedCopy >= config.timing.fixedDeltaTime
			 && numFixed < config.timing.maxFixedUpdates)
		{
			accumulatedCopy -= config.timing.fixedDeltaTime;
			++numFixed;
		}

		if (numFixed >= config.timing.maxFixedUpdates) {
			#ifdef BLACKTHORN_DEBUG
				BT_WARN("Timing: Fixed update count capped at {}", numFixed);
			#endif

			accumulated = 0.0f;
		} else {
			accumulated = accumulatedCopy;
		}

		{
			PROFILE_SCOPE("Fixed Update Loop");
			for (int i = 0; i < numFixed; ++i) {
				PROFILE_SCOPE("Fixed Update");
				fixedUpdate(config.timing.fixedDeltaTime);
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

		jobSystem->flushMainThread();

		const float alpha = accumulated / config.timing.fixedDeltaTime;

		{
			PROFILE_SCOPE("Render");
			render(alpha);
		}

		if (
			settings.get<bool>("graphics", "frame_cap") &&
			!settings.get<bool>("window", "vsync")
		) {
			const float target = 1.0f / settings.get<int>("graphics", "target_fps");
			const U64 end = SDL_GetPerformanceCounter();
			const float elapsed = static_cast<float>(end - now) / freq;
			const float sleepMs = (target - elapsed - 0.002f) * 1000.0f;

			if (sleepMs > 0.0f)
				SDL_Delay(static_cast<U32>(sleepMs));

			while (static_cast<float>(
				SDL_GetPerformanceCounter() - now) / freq < target) {}
		}

		#ifdef BLACKTHORN_DEBUG
			static float logTimer = 0.0f;
			logTimer += frameTime;
			profiler.endFrame();

			if (logTimer >= config.debug.profilingLogInterval) {
				logProfilingInfo();
				logTimer = 0.0f;
			}
		#endif
	}
}

void Engine::render(float alpha) {
	renderer->beginScene();
	getClientSceneManager().render(alpha);
	renderer->endScene();
	SDL_GL_SwapWindow(window);
}

void Engine::update(float dt) {
	EngineCore::update(dt);
	inputManager.update(dt);
}

void Engine::processEvents() {
	auto& settings = Core::Settings::instance();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		inputManager.handleEvent(event);

		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;

			case SDL_EVENT_WINDOW_RESIZED: {
				int pw, ph;
				SDL_GetWindowSizeInPixels(window, &pw, &ph);

				if (pw != config.window.width || ph != config.window.height) {
					config.window.width = pw;
					config.window.height = ph;

					glViewport(0, 0, pw, ph);
					renderer->setProjection(pw, ph);
					UI::UIManager::onWindowResize(pw, ph);

					if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)) {
						settings.set<int>("window", "width", pw);
						settings.set<int>("window", "height", ph);
						settings.saveToFile(config.settingsFilePath);
					}
				}
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				if (settings.get<bool>("window", "fullscreen"))
					SDL_SetWindowFullscreen(window, true);
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
					SDL_SetWindowFullscreen(window, false);
				break;

			case SDL_EVENT_WINDOW_MAXIMIZED:
				settings.set<bool>("window", "maximized", true);
				settings.saveToFile(config.settingsFilePath);
				break;

			case SDL_EVENT_WINDOW_RESTORED:
				settings.set<bool>("window", "maximized", false);
				settings.saveToFile(config.settingsFilePath);
				break;

			case SDL_EVENT_WINDOW_MOVED: {
				if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)) {
					int x, y;
					SDL_GetWindowPosition(window, &x, &y);
					settings.set("window", "pos_x", x);
					settings.set("window", "pos_y", y);
					settings.saveToFile(config.settingsFilePath);
				}
				break;
			}

			default:
				break;
		}
	}
}

void Engine::registerDefaultSettings(Core::Settings& s) {
	EngineCore::registerDefaultSettings(s);

	s.setDefault("window", "width", config.window.width);
	s.setDefault("window", "height", config.window.height);
	s.setDefault("window", "fullscreen", false);
	s.setDefault("window", "vsync", false);
	s.setDefault("window", "maximized", false);
	s.setDefault("window", "pos_x", SDL_WINDOWPOS_CENTERED);
	s.setDefault("window", "pos_y", SDL_WINDOWPOS_CENTERED);

	s.setDefault("graphics", "frame_cap", false);
	s.setDefault("graphics", "target_fps", 60);
	s.setDefault("graphics", "msaa_samples", 0);
	s.setDefault("graphics", "post_processing", true);
	s.setDefault("graphics", "brightness", 1.0f);
	s.setDefault("graphics", "contrast", 1.0f);
	s.setDefault("graphics", "saturation", 1.0f);
	s.setDefault("graphics", "gamma", 1.0f);
	s.setDefault("graphics", "vignette", false);
	s.setDefault("graphics", "vignette_intensity",0.5f);

	s.setDefault("ui", "scale", 1.0f);

	s.setDefault("audio", "master_volume", 1.0f);
	s.setDefault("audio", "music_volume", 1.0f);
	s.setDefault("audio", "sfx_volume", 1.0f);
}

void Engine::applyEngineSettings() {
	EngineCore::applyCoreSettings();

	auto& s = Core::Settings::instance();

	UI::UIManager::setGlobalUIScale(s.get<float>("ui", "scale", 1.0f));

	bool fullscreen = s.get<bool>("window", "fullscreen", false);
	bool maximized = s.get<bool>("window", "maximized", false);

	int width = s.get<int>("window", "width");
	int height = s.get<int>("window", "height");
	int posX = s.get<int>("window", "pos_x", SDL_WINDOWPOS_CENTERED);
	int posY = s.get<int>("window", "pos_y", SDL_WINDOWPOS_CENTERED);

	if (posX != SDL_WINDOWPOS_CENTERED && posY != SDL_WINDOWPOS_CENTERED) {
		int numDisplays;
		SDL_GetDisplays(&numDisplays);
		int displayIndex = 0;

		for (int i = 0; i < numDisplays; ++i) {
			SDL_Rect bounds;
			if (SDL_GetDisplayBounds(i, &bounds)) {
				if (posX >= bounds.x && posX < bounds.x + bounds.w &&
					posY >= bounds.y && posY < bounds.y + bounds.h) {
					displayIndex = i;
					break;
				}
			}
		}

		SDL_Rect usable;
		if (SDL_GetDisplayUsableBounds(displayIndex, &usable)) {
			const int margin = 50;
			posX = std::max(usable.x - width + margin,
				std::min(posX, usable.x + usable.w - margin));
			posY = std::max(usable.y - height + margin,
				std::min(posY, usable.y + usable.h - margin));
		}
	}

	SDL_SetWindowFullscreen(window, false);
	SDL_RestoreWindow(window);
	SDL_SetWindowSize(window, width, height);
	SDL_SetWindowPosition(window, posX, posY);

	if (fullscreen) {
		SDL_SetWindowFullscreen(window, true);
	} else if (maximized) {
		SDL_MaximizeWindow(window);
	}

	SDL_GL_SetSwapInterval(s.get<bool>("window", "vsync") ? 1 : 0);

	audioManager->setCategoryVolume(
		Audio::AudioCategory::Master,
		s.get<float>("audio", "master_volume")
	);

	audioManager->setCategoryVolume(
		Audio::AudioCategory::Music,
		s.get<float>("audio", "music_volume")
	);

	audioManager->setCategoryVolume(
		Audio::AudioCategory::SFX,
		s.get<float>("audio", "sfx_volume")
	);

	inputManager.loadBindingsFromSettings();
}

void Engine::applyPostProcessing() {
	if (!renderer)
		return;

	auto& s = Core::Settings::instance();

	renderer->setPostProcessingEnabled(
		s.get<bool>("graphics", "post_processing", true)
	);

	auto& shader = renderer->getScreenShader();
	shader.setFloat("u_Brightness", s.get<float>("graphics", "brightness", 1.0f));
	shader.setFloat("u_Contrast", s.get<float>("graphics", "contrast", 1.0f));
	shader.setFloat("u_Saturation", s.get<float>("graphics", "saturation", 1.0f));
	shader.setFloat("u_GammaCorrect", s.get<float>("graphics", "gamma", 1.0f));
	shader.setBool ("u_Vignette", s.get<bool> ("graphics", "vignette", false));
	shader.setFloat("u_VignetteIntensity", s.get<float>("graphics", "vignette_intensity", 0.5f));
}

void Engine::registerEngineCallbacks(Core::Settings& s) {
	EngineCore::registerEngineCallbacks(s);

	s.onChange("ui", "scale", [](const std::string& val) {
		try { UI::UIManager::setGlobalUIScale(std::stof(val)); }
		catch (...) {}
	});

	s.onChange("window", "vsync", [](const std::string& val) {
		SDL_GL_SetSwapInterval(val == "true" ? 1 : 0);
	});

	s.onChange("audio", "master_volume", [this](const std::string& val) {
		try {
			audioManager->setCategoryVolume(Audio::AudioCategory::Master, std::stof(val));
		} catch (...) {}
	});

	s.onChange("audio", "music_volume", [this](const std::string& val) {
		try {
			audioManager->setCategoryVolume(Audio::AudioCategory::Music, std::stof(val));
		} catch (...) {}
	});

	s.onChange("audio", "sfx_volume", [this](const std::string& val) {
		try {
			audioManager->setCategoryVolume(Audio::AudioCategory::SFX, std::stof(val));
		} catch (...) {}
	});

	auto applyPP = [this](const std::string&) { applyPostProcessing(); };

	for (const char* key : {
		"post_processing", "brightness", "contrast",
		"saturation", "gamma", "vignette", "vignette_intensity"
	}) {
		s.onChange("graphics", key, applyPP);
	}
}

void Engine::logEngineInfo() {
	std::string glVersion =
		reinterpret_cast<const char*>(glGetString(GL_VERSION));
	std::string rd =
		reinterpret_cast<const char*>(glGetString(GL_RENDERER));

	GLint maxTex = 0, maxAttribs = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);

	int depthBits = 0, stencilBits = 0, msaaSamples = 0;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthBits);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencilBits);
	SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaaSamples);

	BT_LOG(
		"Engine: Graphics Info\n"
		"    OpenGL: {}\n"
		"    Renderer: {}\n"
		"    Max Texture Size: {}\n"
		"    Max Vertex Attributes: {}\n"
		"    Depth: {} bits | Stencil: {} bits | MSAA: {}x\n"
		"    SIMD: {}",
		glVersion,
		rd,
		maxTex,
		maxAttribs,
		depthBits,
		stencilBits,
		msaaSamples,
		getSIMDInfo()
	);
}

} // namespace Blackthorn