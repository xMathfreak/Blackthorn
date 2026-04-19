#pragma once

#include "Graphics/Renderer.h"
#include "Scene/ISceneContext.h"

namespace Blackthorn::Scene {

/**
 * @brief Concrete `ISceneContext` implementation owned by `Engine`.
 *
 * Extends `ISceneContext` (which extends `ISimContext`) with renderer
 * access. Only instantiated in the graphics-enabled client build — the
 * server uses `SimContextImpl` directly via `ISimContext`.
 */
class SceneContextImpl : public ISceneContext {
	Assets::AssetManager& assets;
	Net::ConnectionManager& connection;
	Input::InputManager& input;
	Jobs::JobSystem& jobs;
	SceneManager& scene;
	Core::SimClock& simClock;
	Graphics::Renderer& renderer;

public:
	SceneContextImpl(
		Assets::AssetManager& am,
		Net::ConnectionManager& cm,
		Input::InputManager& im,
		Jobs::JobSystem& js,
		SceneManager& sm,
		Core::SimClock& clock,
		Graphics::Renderer& ren
	)
		: assets(am)
		, connection(cm)
		, input(im)
		, jobs(js)
		, scene(sm)
		, simClock(clock)
		, renderer(ren)
	{}

	Assets::AssetManager& getAssetManager() override { return assets; }
	Net::ConnectionManager& getConnectionManager() override { return connection; }
	Input::InputManager& getInputManager() override { return input; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
	Core::SimClock& getSimClock() override { return simClock; }
	Graphics::Renderer& getRenderer() override { return renderer; }
};

} // namespace Blackthorn::Scene