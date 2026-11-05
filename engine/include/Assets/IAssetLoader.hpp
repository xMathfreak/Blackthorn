#pragma once

#include <memory>
#include <vector>

#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"

namespace Blackthorn::Assets {

class AssetManager;
class AssetResolver;

/**
 * @class IAssetLoader
 * @brief Synchronous loader for AssetType.
 *
 * AssetManager calls setResolver() during registerLoader(), before any load.
 * Loaders read asset bytes through resolver(), so they behave the same in loose
 * and pack builds. resolver() is null only if a loader is used without being
 * registered, and loaders should check for that.
 */
template <typename AssetType>
class IAssetLoader {
public:
	virtual ~IAssetLoader() = default;
	virtual std::unique_ptr<AssetType> load(const LoadParams& params) = 0;

	/// Called by AssetManager at registration. Not part of the loader's public contract.
	void setResolver(AssetResolver* r) { resolver = r; }

protected:
	/// Set by AssetManager::registerLoader. Null until registration.
	AssetResolver* resolver = nullptr;
};

/**
 * @class IAsyncAssetLoader
 * @brief Two-stage loader for AssetType.
 *
 * loadRaw() runs on a worker thread and must not touch GPU state. upload() runs
 * on the main thread and creates the final asset. Like IAssetLoader, the
 * resolver is injected by AssetManager::registerLoader.
 */
template <typename AssetType>
class IAsyncAssetLoader {
public:
	virtual ~IAsyncAssetLoader() = default;

	virtual std::unique_ptr<IRawAssetData> loadRaw(const LoadParams& params) = 0;
	virtual void upload(IRawAssetData& raw, AssetManager& manager) = 0;

	/// Called by AssetManager at registration. Not part of the loader's public contract.
	void setResolver(AssetResolver* r) { resolver = r; }

protected:
	/// Set by AssetManager::registerLoader. Null until registration.
	AssetResolver* resolver = nullptr;
};

} // namespace Blackthorn::Assets
