#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetManager.h"

namespace Blackthorn::Assets {

template <typename AssetType>
AssetStatus AssetHandle<AssetType>::status() const {
	if (!manager)
		return AssetStatus::Missing;

	return manager->statusOf<AssetType>(id);
}

template <typename AssetType>
AssetType* AssetHandle<AssetType>::get() const {
	if (!manager)
		return nullptr;

	return manager->peek<AssetType>(id);
}

} // namespace Blackthorn::Assets
