#include "Assets/AssetDirectoryCache.h"

#include <algorithm>

#include "Assets/AssetRegistry.h"

namespace Blackthorn::Editor::Assets {

namespace {

std::string lowerExtension(const std::filesystem::path& path) {
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext;
}

} // namespace

void AssetDirectoryCache::refresh(const std::filesystem::path& assetsRoot) {
	cached.clear();
	rootNode = AssetTreeNode{};
	stale = false;

	std::error_code existsEc;
	if (!std::filesystem::exists(assetsRoot, existsEc))
		return;

	const auto& registry = AssetRegistry::instance();

	std::error_code iterEc;
	for (const auto& dirEntry :
		std::filesystem::recursive_directory_iterator(assetsRoot, iterEc))
	{
		if (iterEc)
			break;

		if (!dirEntry.is_regular_file())
			continue;

		AssetEntry item;
		item.absolutePath = dirEntry.path();
		item.relativePath = std::filesystem::relative(dirEntry.path(), assetsRoot);
		item.extension = lowerExtension(dirEntry.path());

		const auto* typeEntry = registry.findByExtension(item.extension);
		item.assetType = typeEntry ? typeEntry->type : std::type_index(typeid(void));

		cached.push_back(std::move(item));
	}

	std::sort(cached.begin(), cached.end(),
		[](const AssetEntry& a, const AssetEntry& b) { return a.relativePath < b.relativePath; });

	for (const auto& entry : cached) {
		AssetTreeNode* node = &rootNode;

		for (const auto& segment : entry.relativePath.parent_path()) {
			const std::string segmentName = segment.string();

			auto it = std::find_if(
				node->directories.begin(), node->directories.end(),
				[&](const AssetTreeNode& n) { return n.name == segmentName; }
			);

			node = (it != node->directories.end())
				? &(*it)
				: &node->directories.emplace_back(AssetTreeNode{ segmentName, {}, {} });
		}

		node->files.push_back(&entry);
	}
}

} // namespace Blackthorn::Editor::Assets