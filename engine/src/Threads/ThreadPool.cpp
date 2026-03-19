#include "Threads/ThreadPool.h"

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
		std::unique_lock<std::mutex> lock(queueMutex);
		stop = true;
	}
	condition.notify_all();

	for (auto& t : workers)
		t.join();
}

size_t ThreadPool::pendingCount() const {
	std::unique_lock<std::mutex> lock(queueMutex);
	return taskQueue.size();
}

void ThreadPool::workerLoop() {
	while (true) {
		TaskPtr task;

		{
			std::unique_lock<std::mutex> lock(queueMutex);
			condition.wait(lock, [this] {
				return stop || !taskQueue.empty();
			});

			if (stop && taskQueue.empty())
				return;

			task = std::move(taskQueue.front());
			taskQueue.pop();
		}

		task->invoke();
	}
}

} // namespace Blackthorn::Threads