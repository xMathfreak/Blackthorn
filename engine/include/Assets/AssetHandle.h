#pragma once

#include "Core/Export.h"

#include <string>

namespace Blackthorn::Assets {

class AssetManager;

template <typename AssetType>
class BLACKTHORN_API AssetHandle {
public:
	AssetHandle()
		: id("")
		, manager(nullptr)
	{}

	AssetHandle(const std::string& assetID, AssetManager* mgr)
		: id(assetID)
		, manager(mgr)
	{}

	AssetType* get() const;

	bool isValid() const { return !id.empty() && manager != nullptr; }

	const std::string& getID() const { return id; }

	AssetType* operator->() const { return get(); }
	AssetType& operator*() const { return *get(); }
	explicit operator bool() const { return isValid() && get() != nullptr; } 

private:
	std::string id;
	AssetManager* manager;
};

} // namesace Blackthorn::Assets