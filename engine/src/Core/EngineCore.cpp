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
		BT_WARN("Engine: Initialization skipped - already initialized");
		return false;
	}

	config = cfg;
	FontConfig::setCurrent(cfg.fonts);

	Threads::ThreadRegistry::instance().registerCurrent("Main");
	Debug::Logger::instance().init(cfg.debug.logger);

	BT_LOG("Engine: Initializing");

	auto& settings = Core::Settings::instance();
	settings.loadFromFile(cfg.settingsFilePath);

	registerDefaultSettings(settings);
	onRegisterSettings(settings);

	if (settings.isDirty())
		settings.saveToFile(cfg.settingsFilePath);

	registerEngineCallbacks(settings);

	simClock = std::make_unique<Core::SimClock>(cfg.timing.fixedDeltaTime);
	simClock->load();

	if (!SDL_Init(SDL_INIT_EVENTS)) {
		BT_ERROR("SDL: Failed to initialize - {}", SDL_GetError());
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
		*jobSystem,
		*sceneManager,
		*simClock
	);

	initialized = true;

	BT_LOG("Engine: Initialization complete [Headless | Tick: {}]", simClock->getCurrentTick());
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
	BT_LOG("Engine: Shutting down");
	Debug::Logger::instance().shutdown();
}

void EngineCore::run() {
	if (!initialized) {
		BT_ERROR("Engine: Cannot run - not initialized");
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
			profiler.endFrame();
		#endif
	}

	if (signalReceived.load(std::memory_order::relaxed))
		BT_LOG("EngineCore: signal received, shutting down cleanly");
}

void EngineCore::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
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
}

void EngineCore::lateUpdate(float dt) {
	sceneManager->lateUpdate(dt);
}

void EngineCore::registerDefaultSettings(Core::Settings& s) {
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
	#ifdef BLACKTHORN_DEBUG
		auto& s = Core::Settings::instance();

		Debug::Logger::instance().setLevel(
			static_cast<Debug::LogLevel>(s.get<int>("developer", "log_level", 3))
		);
	#endif
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