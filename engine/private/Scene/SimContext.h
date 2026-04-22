#pragma once

#include "Core/SimClock.h"
#include "Scene/ISimContext.h"

namespace Blackthorn::Scene {

/**
 * @brief Concrete `ISimContext` implementation owned by `EngineCore`.
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

public:
	SimContextImpl(
		Assets::AssetManager& am,
		Net::ConnectionManager& cm,
		Jobs::JobSystem& js,
		SceneManager& sm,
		Core::SimClock& clock
	)
		: assets(am)
		, connection(cm)
		, jobs(js)
		, scene(sm)
		, simClock(clock)
	{}

	Assets::AssetManager& getAssetManager() override { return assets; }
	Net::ConnectionManager& getConnectionManager() override { return connection; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
	Core::SimClock& getSimClock() override { return simClock; }
};

} // namespace Blackthorn::Scene