#pragma once

#include <string>
#include <vector>

namespace Blackthorn::Editor::Assets {

struct AssetTreeNode {
	std::string name;
	std::vector<AssetTreeNode> directories;
	std::vector<size_t> fileIndices;
};

} // namespace Blackthorn::Editor::Assets