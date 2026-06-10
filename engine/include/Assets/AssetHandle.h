#pragma once

#include <string>
#include <thread>

#include "Jobs/JobHandle.h"

namespace Blackthorn::Assets {

class AssetManager;

template <typename AssetType>
class AssetHandle {
public:
	AssetHandle() = default;

	AssetHandle(std::string assetID, AssetManager* mgr, Jobs::JobHandlePtr hdl)
		: id(std::move(assetID))
		, manager(mgr)
		, handle(hdl)
	{}

	bool isReady() const {
		return handle && handle->isComplete();
	}

	bool isValid() const {
		return !id.empty() && manager != nullptr;
	}

	AssetType* get() const;

	void wait() const {
		while (!isReady())
			std::this_thread::yield();
	}

	const std::string& getID() const { return id; }

	AssetType* operator->() const { return get(); }
	AssetType& operator*() const { return *get(); }
	explicit operator bool() const { return isReady() && get() != nullptr; }

private:
	std::string id;
	AssetManager* manager = nullptr;
	Jobs::JobHandlePtr handle = nullptr;
};

} // namespace Blackthorn::Assets