#include "Assets/AssetRegistry.h"

namespace Blackthorn::Editor::Assets {

AssetRegistry& AssetRegistry::instance() {
	static AssetRegistry reg;
	return reg;
}

} // namespace Blackthorn::Editor::Assets