#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "Core/Export.h"

namespace Blackthorn::Assets {

class AssetManager;

template <typename AssetType>
class BLACKTHORN_API AssetHandle {
public:
	AssetHandle() = default;

	AssetHandle(std::string assetID, AssetManager* mgr, std::shared_ptr<std::atomic<bool>> ready)
		: id(std::move(assetID))
		, manager(mgr)
		, readyFlag(std::move(ready))
	{}

	bool isReady() const {
		return readyFlag && readyFlag->load(std::memory_order_acquire);
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
	std::shared_ptr<std::atomic<bool>> readyFlag;
};

} // namespace Blackthorn::Assets