#pragma once

#include "State/EditorContext.h"

namespace Blackthorn {

namespace Assets {
	class AssetManager;
}

namespace Editor::Panels {

class AssetBrowser {
public:
	void draw(State::Context& context, Blackthorn::Assets::AssetManager& manager);
};

} // namespace Editor::Panels

} // namespace Blackthorn