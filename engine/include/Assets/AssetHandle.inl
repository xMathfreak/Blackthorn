#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetManager.h"

namespace Blackthorn::Assets {

template <typename AssetType>
AssetStatus AssetHandle<AssetType>::status() const {
	if (!manager)
		return AssetStatus::Missing;

	return manager->status(ref);
}

template <typename AssetType>
AssetType* AssetHandle<AssetType>::get() const {
	return manager ? manager->resolve(ref) : nullptr;
}

} // namespace Blackthorn::Assets
