#pragma once

#include "Core/Export.h"

namespace Blackthorn {

namespace Assets { class AssetManager; }
namespace Graphics { class Renderer; }
namespace Input { class InputManager; }

namespace Scene {

class SceneManager;

class BLACKTHORN_API ISceneContext {
public:
	virtual ~ISceneContext() = default;

	virtual Assets::AssetManager& getAssetManager() = 0;
	virtual Graphics::Renderer& getRenderer() = 0;
	virtual Input::InputManager& getInputManager() = 0;
	virtual SceneManager& getSceneManager() = 0;
};

} // namespace Scene

} // namespace Blackthorn