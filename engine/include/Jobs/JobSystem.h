#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "Core/Export.h"
#include "Jobs/Job.h"
#include "Jobs/JobHandle.h"
#include "Jobs/JobQueue.h"

namespace Blackthorn::Jobs {

/**
 * @brief Central scheduler for the engine job system.
 *
 * Owns a pool of worker threads, one Chase-Lev work-stealing deque per
 * worker, and a dedicated MPSC queue for main-thread-pinned jobs.
 *
 * The main-thread queue uses a sentinel-node MPSC design (Michael & Scott
 * queue, enqueue side only) to eliminate the initialisation race that
 * occurs when the queue transitions from empty to non-empty with two
 * independent head/tail atomics.
 *
 * Typical frame usage:
 * @code
 *   auto physicsHandle = js.createHandle();
 *   physicsHandle->addPending(count - 1);
 *
 *   for (int i = 0; i < count; ++i)
 *       js.submit(Job(lambda, physicsHandle));
 *
 *   js.submit(Job(renderLambda, nullptr, physicsHandle,
 *                 ThreadAffinity::MainThread));
 *
 *   // At the render sync point:
 *   js.flushMainThread();
 * @endcode
 *
 * @note workerCount == 0 selects max(1, hardware_concurrency - 1).
 */
class BLACKTHORN_API JobSystem {
public:
	explicit JobSystem(size_t workerCount = 0);
	~JobSystem();

	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	/**
	 * @brief Creates a new JobHandle with a pending count of 1.
	 */
	JobHandlePtr createHandle();

	/**
	 * @brief Submits a job to the system.
	 *
	 * If the job has an incomplete dependency it is stored as a continuation
	 * and enqueued automatically when the dependency completes. Otherwise it
	 * is enqueued immediately onto the appropriate queue.
	 */
	void submit(Job job);

	/**
	 * @brief Drains and executes all pending main-thread jobs.
	 *
	 * Processes jobs until the main-thread queue is empty at the time of
	 * the call, including any jobs enqueued by completions during execution.
	 *
	 * @note Must be called from the main thread.
	 */
	void flushMainThread();

	/**
	 * @brief Blocks until a handle is complete, participating in work
	 * stealing while waiting.
	 *
	 * @note Avoid calling from the main thread outside of loading screens.
	 */
	void wait(const JobHandlePtr& handle);

	size_t workerCount() const { return workers.size(); }

private:
	void enqueueReady(Job&& fn);
	bool executeOne(bool mainThreadOnly = false);
	void workerLoop(size_t workerIndex);

	std::vector<std::unique_ptr<JobQueue>> queues;
	std::vector<std::thread> workers;

	alignas(64) std::atomic<bool> shutdown { false };
	alignas(64) std::atomic<size_t> activeJobs { 0 };
	alignas(64) std::atomic<size_t> nextWorker { 0 };

	static int getWorkerIndex();
	static void setWorkerIndex(int idx);

	struct MainThreadNode {
		Job job;
		std::atomic<MainThreadNode*> next { nullptr };

		MainThreadNode() = default;

		explicit MainThreadNode(Job&& j)
			: job(std::move(j))
		{}
	};

	std::atomic<MainThreadNode*> mainHead { nullptr };
	std::atomic<MainThreadNode*> mainTail { nullptr };
};

} // namespace Blackthorn::Jobs