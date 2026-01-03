#pragma once

#include "Core/Export.h"

#include <memory>
#include <vector>

namespace Blackthorn::Assets {

template <typename AssetType>
class BLACKTHORN_API IAssetLoader {
public:
	virtual ~IAssetLoader() = default;
	virtual std::unique_ptr<AssetType> load(const std::string& path) = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;
};

} // namespace Blackthorn::Assets