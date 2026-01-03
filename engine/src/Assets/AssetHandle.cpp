#include "Assets/AssetHandle.h"
#include "Assets/AssetManager.h"

template <typename AssetType>
AssetType* Blackthorn::Assets::AssetHandle<AssetType>::get() const {
	if (!isValid())
		return nullptr;

	return manager->get<AssetType>(id);
}