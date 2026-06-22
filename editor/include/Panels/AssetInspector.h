#pragma once

#include "State/EditorContext.h"

namespace Blackthorn {

namespace Assets {
	class AssetManager;
}

namespace Editor::Panels {

class AssetInspector {
public:
	void draw(State::Context& context, Blackthorn::Assets::AssetManager& manager);

private:
	std::optional<Assets::AssetEntry> lastEntry;
};

} // namespace Editor::Panels

} // namespace Blackthorn