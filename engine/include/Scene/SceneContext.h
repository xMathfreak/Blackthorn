#pragma once

#include "Scene/ISceneContext.h"

namespace Blackthorn::Scene {

class SceneContextImpl : public ISceneContext {
	Assets::AssetManager& assets;
	Graphics::Renderer& renderer;
	Input::InputManager& input;
	SceneManager& scene;

public:
	SceneContextImpl(
		Assets::AssetManager& am,
		Graphics::Renderer& ren,
		Input::InputManager& im,
		SceneManager& sm
	)
		: assets(am)
		, renderer(ren)
		, input(im)
		, scene(sm)
	{}

	Assets::AssetManager& getAssetManager() override{ return assets; }
	Graphics::Renderer& getRenderer() override{ return renderer; }
	Input::InputManager& getInputManager() override{ return input; }
	SceneManager& getSceneManager() override{ return scene; }
};

} // namespace Blackthorn::Scene