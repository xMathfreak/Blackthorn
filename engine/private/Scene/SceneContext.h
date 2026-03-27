#include "Scene/ISceneContext.h"

namespace Blackthorn::Scene {

class SceneContextImpl : public ISceneContext {
	Assets::AssetManager& assets;
	Graphics::Renderer& renderer;
	Input::InputManager& input;
	Jobs::JobSystem& jobs;
	SceneManager& scene;

public:
	SceneContextImpl(
		Assets::AssetManager& am,
		Graphics::Renderer& ren,
		Input::InputManager& im,
		Jobs::JobSystem& js,
		SceneManager& sm
	)
		: assets(am)
		, renderer(ren)
		, input(im)
		, jobs(js)
		, scene(sm)
	{}

	Assets::AssetManager& getAssetManager() override { return assets; }
	Graphics::Renderer& getRenderer() override { return renderer; }
	Input::InputManager& getInputManager() override { return input; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
};

} // namespace Blackthorn::Scene