#include "Threads/ThreadPool.h"

#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Threads {

ThreadPool::ThreadPool(size_t workerCount) {
	if (workerCount == 0) {
		const size_t hw = std::thread::hardware_concurrency();
		workerCount = (hw > 1) ? (hw - 1) : 1;
	}

	workers.reserve(workerCount);
	for (size_t i = 0; i < workerCount; ++i)
		workers.emplace_back(&ThreadPool::workerLoop, this);
}

ThreadPool::~ThreadPool() {
	{
		std::unique_lock lock(queueMutex);
		stop.store(true, std::memory_order_release);
	}
	condition.notify_all();

	for (auto& t : workers)
		t.join();
}

size_t ThreadPool::pendingCount() const {
	std::unique_lock lock(queueMutex);
	return taskQueue.size();
}

void ThreadPool::workerLoop() {
	static std::atomic<int> workerIdx{0};
	const int idx = workerIdx.fetch_add(1, std::memory_order_relaxed);

	ThreadRegistry::instance().registerCurrent("Worker-" + std::to_string(idx));

	while (true) {
		TaskPtr task;

		{
			std::unique_lock lock(queueMutex);
			condition.wait(lock, [this] {
				return stop.load(std::memory_order_relaxed) || !taskQueue.empty();
			});

			if (stop.load(std::memory_order_relaxed) && taskQueue.empty())
				break;

			task = std::move(taskQueue.front());
			taskQueue.pop();
		}

		task->invoke();
	}

	ThreadRegistry::instance().unregisterCurrent();
}

} // namespace Blackthorn::Threads