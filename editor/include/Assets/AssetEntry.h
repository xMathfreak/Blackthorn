#pragma once

#include <filesystem>
#include <typeindex>

namespace Blackthorn::Editor::Assets {

struct AssetEntry {
	std::filesystem::path absolutePath;
	std::filesystem::path relativePath;
	std::string extension;

	std::type_index assetType = std::type_index(typeid(void));
	mutable void* cachedPtr = nullptr;
};

} // namespace Blackthorn::Editor::Assets