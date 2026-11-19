#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Jobs/Job.hpp"

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
		std::scoped_lock lock(mtx);
		queue.push_back(std::move(job));
	}

	std::unique_ptr<Job> pop() {
		std::scoped_lock lock(mtx);

		if (queue.empty())
			return nullptr;

		auto job = std::move(queue.front());
		queue.pop_front();
		return job;
	}

	std::unique_ptr<Job> steal() {
		std::scoped_lock lock(mtx);
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