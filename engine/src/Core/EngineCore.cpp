#include "Core/EngineCore.h"

#include "Core/Settings.h"
#include "Core/SimClock.h"
#include "Debug/Logger.h"
#include "Debug/Profiler.h"
#include "Net/Transport/Sockets/SocketFactory.h"
#include "Scene/SimContext.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn {

void EngineCore::installSignalHandlers() {
	std::signal(SIGINT, EngineCore::signalHandler);
	std::signal(SIGTERM, EngineCore::signalHandler);
}

void EngineCore::signalHandler(int) {
	signalReceived.store(true, std::memory_order::relaxed);
}

EngineCore::EngineCore() {}

EngineCore::~EngineCore() {
	shutdown();
}

bool EngineCore::init(const EngineConfig& cfg) {
	if (initialized) {
		BT_WARN("EngineCore already initialized.");
		return false;
	}

	config = cfg;
	FontConfig::setCurrent(cfg.fonts);

	Threads::ThreadRegistry::instance().registerCurrent("Main");
	Debug::Logger::instance().init(cfg.debug.logger);

	auto& settings = Core::Settings::instance();
	settings.loadFromFile(cfg.settingsFilePath);

	registerEngineDefaults(settings);
	onRegisterSettings(settings);

	if (settings.isDirty())
		settings.saveToFile(cfg.settingsFilePath);

	registerEngineCallbacks(settings);

	simClock = std::make_unique<Core::SimClock>(cfg.timing.fixedDeltaTime);
	simClock->load();

	if (!SDL_Init(SDL_INIT_EVENTS)) {
		BT_ERROR("SDL_Init failed: {}", SDL_GetError());
		cleanupInitialization();
		return false;
	}

	Net::Transport::Sockets::SocketFactory::init();

	assetManager = std::make_unique<Assets::AssetManager>(
		cfg.threading.assetWorkerCount);

	jobSystem = std::make_unique<Jobs::JobSystem>(
		cfg.threading.jobWorkerCount);

	applyCoreSettings();

	sceneManager = std::make_unique<Scene::SceneManager>();

	simContext = std::make_unique<Scene::SimContextImpl>(
		*assetManager,
		*connectionManager,
		inputManager,
		*jobSystem,
		*sceneManager,
		*simClock
	);

	initialized = true;

	BT_LOG("EngineCore initialized (tick {})", simClock->getCurrentTick());
	return true;
}

void EngineCore::shutdown() {
	if (!initialized)
		return;

	auto& s = Core::Settings::instance();

	if (simClock)
		simClock->save();

	if (s.isDirty())
		s.saveToFile(config.settingsFilePath);

	if (assetManager)
		assetManager->clear();

	SDL_Quit();

	initialized = false;
	running = false;
	Debug::Logger::instance().shutdown();
}

void EngineCore::run() {
	if (!initialized) {
		BT_ERROR("Cannot run: EngineCore not initialized");
		return;
	}

	installSignalHandlers();

	Uint64 lastTime = SDL_GetPerformanceCounter();
	float accumulated = 0.0f;
	const float freq = static_cast<float>(SDL_GetPerformanceFrequency());

	auto& settings = Core::Settings::instance();
	running = true;

	#ifdef BLACKTHORN_DEBUG
		auto& profiler = Debug::Profiler::instance();
	#endif

	while (running && !signalReceived.load(std::memory_order::relaxed)) {
		#ifdef BLACKTHORN_DEBUG
			profiler.beginFrame();
			PROFILE_SCOPE("Frame");
		#endif

		const Uint64 now = SDL_GetPerformanceCounter();
		float frameTime = static_cast<float>(now - lastTime) / freq;
		lastTime = now;

		{
			PROFILE_SCOPE("Events");
			processEvents();
		}

		if (frameTime > config.timing.maxDeltaTime) {
			BT_WARN("EngineCore: frame time capped {:.3f} -> {:.3f}",
				frameTime, config.timing.maxDeltaTime);
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
				BT_WARN("EngineCore: fixed update capped at {}", numFixed);
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

		if (settings.get<bool>("graphics", "frame_cap") &&
		 !settings.get<bool>("window", "vsync"))
		{
			const float target = 1.0f / settings.get<int>("graphics", "target_fps");
			const Uint64 end = SDL_GetPerformanceCounter();
			const float elapsed = static_cast<float>(end - now) / freq;
			const float sleepMs = (target - elapsed - 0.002f) * 1000.0f;

			if (sleepMs > 0.0f)
				SDL_Delay(static_cast<Uint32>(sleepMs));

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

	if (signalReceived.load(std::memory_order::relaxed))
		BT_LOG("EngineCore: signal received, shutting down cleanly");
}

void EngineCore::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		inputManager.handleEvent(event);

		if (event.type == SDL_EVENT_QUIT)
			running = false;
	}
}

void EngineCore::fixedUpdate(float dt) {
	simClock->tick();
	sceneManager->fixedUpdate(dt, simClock->getCurrentTick());
}

void EngineCore::update(float dt) {
	assetManager->flushPendingUploads(config.assets.uploadBudget);
	connectionManager->poll(jobSystem.get());
	sceneManager->update(dt);
	inputManager.update(dt);
}

void EngineCore::lateUpdate(float dt) {
	sceneManager->lateUpdate(dt);
}

void EngineCore::registerEngineDefaults(Core::Settings& s) {
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

	s.setDefault<Uint64>("simulation", "tick", static_cast<Uint64>(0));

	#ifdef BLACKTHORN_DEBUG
		s.setDefault("developer", "log_level", 3);
	#endif

	s.setDefault("developer", "worker_threads", 0);
}

void EngineCore::registerEngineCallbacks(Core::Settings& s) {
	#ifdef BLACKTHORN_DEBUG
		s.onChange("developer", "log_level", [](const std::string& val) {
			try {
				Debug::Logger::instance().setLevel(
					static_cast<Debug::LogLevel>(std::stoi(val)));
			} catch (...) {}
		});
	#endif
}

void EngineCore::applyCoreSettings() {
	auto& s = Core::Settings::instance();

	#ifdef BLACKTHORN_DEBUG
		Debug::Logger::instance().setLevel(
			static_cast<Debug::LogLevel>(s.get<int>("developer", "log_level", 3))
		);
	#endif

	inputManager.loadBindingsFromSettings();
}

void EngineCore::cleanupInitialization() {
	SDL_Quit();
}

#ifdef BLACKTHORN_DEBUG
void EngineCore::logProfilingInfo() {
	auto& profiler = Debug::Profiler::instance();

	SDL_Log("Frame Time: %.2f ms (%.1f FPS)",
		profiler.getAverageFrameTime(60),
		1000.0f / profiler.getAverageFrameTime(60)
	);

	for (const auto& name : profiler.getAllScopeNames()) {
		auto stats = profiler.getStats(name, 60);
		if (stats.average > 0.1f) {
			SDL_Log(" %s: avg=%.2f min=%.2f max=%.2f calls=%d",
				name.c_str(),
				stats.average, stats.min, stats.max, stats.callCount
			);
		}
	}
}
#endif

} // namespace Blackthorn