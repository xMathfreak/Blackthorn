#include "Assets/AssetManager.h"

namespace Blackthorn::Assets {

AssetManager::AssetManager(Jobs::JobSystem& js)
	: jobs(js)
{}

void AssetManager::shutdown() {
	flushAllPendingUploads();
}

void AssetManager::flushPendingUploads() {
	jobs.flushMainThread();
}

void AssetManager::flushAllPendingUploads() {
	while (pendingTotal.load(std::memory_order::acquire) > 0) {
		jobs.flushMainThread();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	jobs.flushMainThread();
}

size_t AssetManager::pendingCount() const {
	return pendingTotal.load(std::memory_order::relaxed);
}

} // namespace Blackthorn::Assets