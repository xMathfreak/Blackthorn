#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include "Core/Export.h"
#include "Jobs/Job.h"

namespace Blackthorn::Jobs {

/**
 * @brief Mutex-protected work queue for one worker thread.
 *
 * The owner pushes and pops at the front. Other workers steal from the back.
 * Unbounded: push() never fails.
 */
class BLACKTHORN_API JobQueue {
public:
	JobQueue() = default;

	JobQueue(const JobQueue&) = delete;
	JobQueue& operator=(const JobQueue&) = delete;

	void push(std::unique_ptr<Job> job) {
		std::lock_guard<std::mutex> lock(mtx);
		queue.push_back(std::move(job));
	}

	std::unique_ptr<Job> pop() {
		std::lock_guard<std::mutex> lock(mtx);
		if (queue.empty())
			return nullptr;

		auto job = std::move(queue.front());
		queue.pop_front();
		return job;
	}

	std::unique_ptr<Job> steal() {
		std::lock_guard<std::mutex> lock(mtx);
		if (queue.empty())
			return nullptr;

		auto job = std::move(queue.back());
		queue.pop_back();
		return job;
	}

private:
	std::deque<std::unique_ptr<Job>> queue;
	std::mutex mtx;
};

} // namespace Blackthorn::Jobs