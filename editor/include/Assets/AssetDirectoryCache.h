#pragma once

#include <filesystem>
#include <vector>

#include "Assets/AssetEntry.h"
#include "Assets/AssetTreeNode.h"

namespace Blackthorn::Editor::Assets {

class AssetDirectoryCache {
public:
	void refresh(const std::filesystem::path& assetsRoot);

	const std::vector<AssetEntry>& entries() const { return cached; }
	const AssetTreeNode& tree() const { return rootNode; }

	bool isStale() const { return stale; }
	void markStale() { stale = true; }

private:
	std::vector<AssetEntry> cached;
	AssetTreeNode rootNode;
	bool stale = true;
};

} // namespace Blackthorn::Editor::Assets