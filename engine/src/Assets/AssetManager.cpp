#include "Assets/AssetManager.h"

namespace Blackthorn::Assets {

AssetManager::AssetManager(Jobs::JobSystem& js)
	: jobs(js)
{
	BT_DEBUG("AssetManager: Initialised");
}

void AssetManager::flushPendingUploads() {
	jobs.flushMainThread();
}

void AssetManager::flushAllPendingUploads() {
	while (pendingTotal.load(std::memory_order::acquire) > 0) {
		jobs.flushMainThread();
		std::this_thread::yield();
	}

	jobs.flushMainThread();
}

size_t AssetManager::pendingCount() const {
	return pendingTotal.load(std::memory_order::relaxed);
}

} // namespace Blackthorn::Assets