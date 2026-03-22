#include "Assets/AssetManager.h"

#include <thread>

namespace Blackthorn::Assets {

AssetManager::AssetManager(size_t workerCount)
	: threadPool(workerCount)
{
	BT_DEBUG(
		"AssetManager initialised ({} worker thread{})",
		threadPool.workerCount(),
		threadPool.workerCount() == 1 ? "" : "s"
	);
}

bool AssetManager::processOneUpload() {
	Threads::TaskPtr task;

	{
		std::unique_lock<std::mutex> lock(uploadMutex);
		if (uploadQueue.empty())
			return false;

		task = std::move(uploadQueue.front());
		uploadQueue.pop();
	}

	task->invoke();
	return true;
}

size_t AssetManager::flushPendingUploads(size_t uploadBudget) {
	size_t processed = 0;
	while (processed < uploadBudget && processOneUpload())
		++processed;
	return processed;
}

void AssetManager::flushAllPendingUploads() {
	while (pendingTotal.load(std::memory_order_acquire) > 0) {
		flushPendingUploads(64);
		std::this_thread::yield();
	}
	while (processOneUpload()) {}
}

size_t AssetManager::pendingCount() const {
	return pendingTotal.load(std::memory_order_relaxed);
}

} // namespace Blackthorn::Assets