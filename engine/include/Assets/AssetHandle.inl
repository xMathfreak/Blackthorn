#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetManager.h"

namespace Blackthorn::Assets {

template <typename AssetType>
AssetType* AssetHandle<AssetType>::get() const {
	if (!isReady())
		return nullptr;

	return manager->get<AssetType>(id);
}

} // namespace Blackthorn::Assets
