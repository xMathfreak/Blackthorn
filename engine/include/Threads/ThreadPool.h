#pragma once

#include <condition_variable>
#include <queue>
#include <thread>

#include "Core/Export.h"
#include "Threads/MoveOnlyTask.h"

namespace Blackthorn::Threads {

/**
 * @brief A fixed size pool of worker threads that execute tasks.
 *
 * Construction:
 *   Pass 0 (default) to use max(1, hardware_concurrency - 1) threads.
 *
 * `enqueue()` accepts any callable — including move-only lambdas that
 * capture unique_ptr, moved structs, etc. — via a forwarding reference
 * template.
 *
 * Thread safety:
 *   `enqueue()` — safe to call from any thread.
 *   The destructor joins all workers; call it only from the main thread once
 *   all producers have stopped enqueuing work.
 */
class BLACKTHORN_API ThreadPool {
public:
	explicit ThreadPool(size_t workerCount = 0);
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	template <typename Callable>
	requires std::invocable<Callable>
	void enqueue(Callable&& callable) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			taskQueue.push(makeTask(std::forward<Callable>(callable)));
		}

		condition.notify_one();
	}

	size_t workerCount() const { return workers.size(); }

	size_t pendingCount() const;

private:
	void workerLoop();

	std::vector<std::thread> workers;
	std::queue<TaskPtr> taskQueue;

	mutable std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<bool> stop{false};
};

} // namespace Blackthorn::Threads
