#pragma once

#include "Core/Export.h"

#include <string>

namespace Blackthorn::Assets {

class BLACKTHORN_API IAsset {
public:
	virtual ~IAsset() = default;

	const std::string& getID() const { return id; }
	const std::string& getPath() const { return path; }

	size_t getMemoryUsage() const { return memoryUsage; }
	bool isLoaded() const { return loaded; }

private:
	friend class AssetManager;

	std::string id;
	std::string path;
	size_t memoryUsage = 0;
	bool loaded = false;
};

} // namespace Blackthorn::Assets