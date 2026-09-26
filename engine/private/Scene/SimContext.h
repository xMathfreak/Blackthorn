#pragma once

#include "Core/SimClock.h"
#include "Scene/ISimContext.h"

namespace Blackthorn::Scene {

/**
 * @brief Concrete `ISimContext` implementation owned by `Runtime`.
 *
 * Holds references to all simulation services. Passed into `Scene::init()`
 * on both the client and the dedicated server build.
 */
class SimContextImpl : public ISimContext {
	Assets::AssetManager& assets;
	Net::ConnectionManager& connection;
	Jobs::JobSystem& jobs;
	SceneManager& scene;
	Core::SimClock& simClock;
	Saves::SaveManager& saves;
	Localization::LocalizationManager& locale;

public:
	SimContextImpl(
		Assets::AssetManager& am,
		Net::ConnectionManager& cm,
		Jobs::JobSystem& js,
		SceneManager& sm,
		Core::SimClock& clock,
		Saves::SaveManager& sv,
		Localization::LocalizationManager& lm
	)
		: assets(am)
		, connection(cm)
		, jobs(js)
		, scene(sm)
		, simClock(clock)
		, saves(sv)
		, locale(lm)
	{}

	Assets::AssetManager& getAssetManager() override { return assets; }
	Net::ConnectionManager& getConnectionManager() override { return connection; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
	Core::SimClock& getSimClock() override { return simClock; }
	Saves::SaveManager& getSaveManager() override { return saves; }
	Localization::LocalizationManager& getLocalizationManager() override { return locale; }
};

} // namespace Blackthorn::Scene