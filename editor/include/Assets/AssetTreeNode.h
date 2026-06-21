#pragma once

#include <string>
#include <vector>

#include "Assets/AssetEntry.h"

namespace Blackthorn::Editor::Assets {

struct AssetTreeNode {
	std::string name;
	std::vector<AssetTreeNode> directories;
	std::vector<const AssetEntry*> files;
};

} // namespace Blackthorn::Editor::Assets