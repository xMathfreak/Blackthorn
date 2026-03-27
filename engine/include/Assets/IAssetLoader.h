#pragma once

#include <memory>
#include <vector>

#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"

namespace Blackthorn::Assets {

class AssetManager;

template <typename AssetType>
class IAssetLoader {
public:
	virtual ~IAssetLoader() = default;
	virtual std::unique_ptr<AssetType> load(const LoadParams& params) = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;
};

template <typename AssetType>
class IAsyncAssetLoader {
public:
	virtual ~IAsyncAssetLoader() = default;

	virtual std::unique_ptr<IRawAssetData> loadRaw(const LoadParams& params) = 0;
	virtual void upload(IRawAssetData& raw, AssetManager& manager) = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;
};

} // namespace Blackthorn::Assets