#pragma once

#include <memory>
#include <vector>

#include "Assets/LoadParams.h"
#include "Core/Export.h"

namespace Blackthorn::Assets {

template <typename AssetType>
class BLACKTHORN_API IAssetLoader {
public:
	virtual ~IAssetLoader() = default;
	virtual std::unique_ptr<AssetType> load(const LoadParams& params) = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;
};

} // namespace Blackthorn::Assets