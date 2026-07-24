#include "Core/EngineCore.h"

#include <algorithm>

#include "Core/Settings.h"
#include "Core/SimClock.h"
#include "Debug/Logger.h"
#include "Debug/Profiler.h"
#include "Net/Transport/Sockets/SocketFactory.h"
#include "Saves/Sections/ClockSaveSection.h"
#include "Saves/Sections/MetaSaveSection.h"
#include "Saves/Sections/WorldSaveSection.h"
#include "Scene/SimContext.h"
#include "Threads/Relax.h"
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

	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, cfg.metadata.name.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, cfg.metadata.version.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, cfg.metadata.identifier.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, cfg.metadata.author.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, cfg.metadata.copyright.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, cfg.metadata.url.c_str());
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, cfg.metadata.type.c_str());
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "Game");

	#ifdef BLACKTHORN_DEBUG
		SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
	#else
		SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
	#endif

	config = cfg;

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

	simClock = std::make_unique<Core::SimClock>(
		cfg.timing.fixedDeltaTime,
		cfg.timing.initialTick
	);

	#ifdef BLACKTHORN_HEADLESS
		simClock->load();
	#endif

	if (!SDL_Init(SDL_INIT_EVENTS)) {
		BT_ERROR("SDL: Failed to initialize - {}", SDL_GetError());
		cleanupInitialization();
		return false;
	}

	jobSystem = std::make_unique<Jobs::JobSystem>(
		cfg.threading.jobWorkerCount
	);

	assetManager = std::make_unique<Assets::AssetManager>(
		*jobSystem
	);

	BT_LOG("AssetManager: Initialized");

	connectionManager = std::make_unique<Net::ConnectionManager>(cfg.net);

	initSaveManager();

	applyCoreSettings();

	sceneManager = std::make_unique<Scene::SceneManager>();

	simContext = std::make_unique<Scene::SimContextImpl>(
		*assetManager,
		*connectionManager,
		*jobSystem,
		*sceneManager,
		*simClock,
		*saveManager
	);

	// Every scene pushed from here on receives *simContext automatically -
	// see SceneManager::setContext() and ISimScene.
	sceneManager->setContext(*simContext);

	initialized = true;

	BT_LOG("Engine: Core initialized (Tick: {})", simClock->getCurrentTick());
	return true;
}

void EngineCore::shutdown() {
	if (!initialized)
		return;

	auto& s = Core::Settings::instance();

	#ifdef BLACKTHORN_HEADLESS
		if (simClock)
			simClock->save();
	#endif

	sceneManager.reset();
	simContext.reset();
	saveManager.reset();

	if (s.isDirty())
		s.saveToFile(config.settingsFilePath);

	connectionManager->stop();

	assetManager->shutdown();
	jobSystem->shutdown();

	Net::Transport::Sockets::SocketFactory::shutdown();

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

	U64 lastTime = SDL_GetPerformanceCounter();
	float accumulated = 0.0f;
	const float freq = static_cast<float>(SDL_GetPerformanceFrequency());

	running = true;

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

		if (frameCapEnabled.get()) {
			const int fps = std::max(targetFPS.get(), 1);
			const float target = 1.0f / static_cast<float>(fps);

			constexpr float spinWindow = 0.001f;

			while (true) {
				const float elapsed = static_cast<float>(SDL_GetPerformanceCounter() - now) / freq;
				const float remaining = target - elapsed;

				if (remaining <= 0.0f)
					break;

				if (remaining > spinWindow)
					SDL_Delay(1);
				else
					Threads::relax();
			}
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
	assetManager->flushPendingUploads();
	connectionManager->poll(jobSystem.get());
	sceneManager->update(dt);
}

void EngineCore::lateUpdate(float dt) {
	sceneManager->lateUpdate(dt);
}

void EngineCore::registerDefaultSettings(Core::Settings& s) {
	#ifdef BLACKTHORN_HEADLESS
		s.setDefault<U64>("simulation", "tick", static_cast<U64>(0));
		s.setDefault("graphics", "frame_cap", true);
		s.setDefault("graphics", "target_fps", 60);
	#endif

	#ifdef BLACKTHORN_DEBUG
		s.setDefault("developer", "log_level", 3);
	#endif

	s.setDefault("developer", "worker_threads", 0);
	s.setDefault("saves", "make_backups", true);
}

void EngineCore::registerEngineCallbacks(Core::Settings& s) {
	frameCapEnabled.attach();
	targetFPS.attach();

	#ifdef BLACKTHORN_DEBUG
		s.onChange("developer", "log_level", [](const std::string& val) {
			try {
				Debug::Logger::instance().setLevel(
					static_cast<Debug::LogLevel>(std::stoi(val)));
			} catch (...) {}
		});
	#endif

	s.onChange("saves", "make_backups", [this](const std::string& val) {
		if (saveManager)
			saveManager->setBackupsEnabled(val == "true" || val == "1");
	});
}

Saves::SaveId EngineCore::getShutdownSaveId() const {
	Saves::SaveId sid;
	sid.id = Core::UUID::makeStable("blackthorn.autosave.shutdown");
	sid.displayName = "autosave_shutdown";
	sid.flags = Saves::SaveFlags::Autosave;
	return sid;
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

void EngineCore::initSaveManager() {
	saveManager = std::make_unique<Saves::SaveManager>(config.save);

	Saves::Sections::WorldSaveSection::registerTypes();

	saveManager->registerSection(
		std::make_unique<Saves::Sections::ClockSaveSection>(*simClock)
	);

	saveManager->registerSection(
		std::make_unique<Saves::Sections::MetaSaveSection>(
			"Blackthorn " +
			std::to_string(BLACKTHORN_VERSION_MAJOR) + "." +
			std::to_string(BLACKTHORN_VERSION_MINOR) + "." +
			std::to_string(BLACKTHORN_VERSION_PATCH)
		)
	);

	const bool makeBackups = Core::Settings::instance().get<bool>(
		"saves", "make_backups", true
	);

	saveManager->setBackupsEnabled(makeBackups);

	onRegisterSaveSections(*saveManager);

	BT_LOG(
		"SaveManager: Ready (dir: '{}', ext: '{}', backup ext: '{}', "
		"compression: {}, encryption: {}, backups: {})",
		config.save.directory,
		config.save.extension,
		config.save.backupExtension,
		config.save.compressionLevel > 0
			? std::to_string(config.save.compressionLevel) : "off",
		config.save.encryptionEnabled ? "on" : "off",
		makeBackups ? "on" : "off"
	);
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