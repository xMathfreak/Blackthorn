#include "Scene/ISceneContext.h"

namespace Blackthorn::Scene {

class SceneContextImpl : public ISceneContext {
	Assets::AssetManager& assets;
	Core::SimClock& simClock;
	Graphics::Renderer& renderer;
	Input::InputManager& input;
	Jobs::JobSystem& jobs;
	SceneManager& scene;

public:
	SceneContextImpl(
		Assets::AssetManager& am,
		Core::SimClock& sc,
		Graphics::Renderer& ren,
		Input::InputManager& im,
		Jobs::JobSystem& js,
		SceneManager& sm
	)
		: assets(am)
		, simClock(sc)
		, renderer(ren)
		, input(im)
		, jobs(js)
		, scene(sm)
	{}

	Assets::AssetManager& getAssetManager() override { return assets; }
	Core::SimClock& getSimClock() override { return simClock; }
	Graphics::Renderer& getRenderer() override { return renderer; }
	Input::InputManager& getInputManager() override { return input; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
};

} // namespace Blackthorn::Scene