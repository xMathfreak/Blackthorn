#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

#include "Core/Export.h"
#include "Jobs/Job.h"

namespace Blackthorn::Jobs {

/**
 * @brief Lock-free work-stealing deque.
 *
 * The owning thread pushes and pops from the bottom (LIFO).
 * Any thread may steal from the top (FIFO).
 *
 * Capacity is a fixed power-of-two set at construction. Jobs are stored
 * by pointer in a circular buffer to keep the deque itself small and
 * avoid copying non-movable types across threads.
 *
 * @note push() and pop() must only be called from the owning thread.
 *       steal() is safe to call from any thread.
 */
class BLACKTHORN_API JobQueue {
public:
	/**
	 * @param capacityLog2 Log2 of the fixed capacity (default 10 → 1024 slots).
	 *                     Must be at least 1.
	 */
	explicit JobQueue(size_t capacityLog2 = 10)
		: mask((size_t(1) << capacityLog2) - 1)
		, buffer(size_t(1) << capacityLog2)
	{}

	/**
	 * @brief Pushes a job onto the bottom of the deque (owner thread only).
	 *
	 * @return false if the deque is full (job was not pushed).
	 */
	bool push(std::unique_ptr<Job> job) {
		const size_t b = bottom.load(std::memory_order_relaxed);
		const size_t t = top.load(std::memory_order_acquire);

		if (b - t >= capacity())
			return false;

		buffer[b & mask].store(job.release(), std::memory_order_relaxed);

		std::atomic_thread_fence(std::memory_order_release);
		bottom.store(b + 1, std::memory_order_relaxed);
		return true;
	}

	/**
	 * @brief Pops a job from the bottom of the deque (owner thread only).
	 *
	 * @return The job, or nullptr if the deque is empty.
	 */
	std::unique_ptr<Job> pop() {
		const size_t b = bottom.load(std::memory_order_relaxed) - 1;
		bottom.store(b, std::memory_order_relaxed);

		std::atomic_thread_fence(std::memory_order_seq_cst);
		size_t t = top.load(std::memory_order_relaxed);

		if (t > b) {
			bottom.store(b + 1, std::memory_order_relaxed);
			return nullptr;
		}

		Job* raw = buffer[b & mask].load(std::memory_order_relaxed);

		if (t == b) {
			if (!top.compare_exchange_strong(
					t, t + 1,
					std::memory_order_seq_cst,
					std::memory_order_relaxed))
			{
				bottom.store(b + 1, std::memory_order_relaxed);
				return nullptr;
			}

			bottom.store(b + 1, std::memory_order_relaxed);
		}

		return std::unique_ptr<Job>(raw);
	}

	/**
	 * @brief Steals a job from the top of the deque (any thread).
	 *
	 * @return The job, or nullptr if the deque is empty or a concurrent
	 *         steal/pop won the race.
	 */
	std::unique_ptr<Job> steal() {
		size_t t = top.load(std::memory_order_acquire);
		std::atomic_thread_fence(std::memory_order_seq_cst);
		const size_t b = bottom.load(std::memory_order_acquire);

		if (t >= b)
			return nullptr;

		Job* raw = buffer[t & mask].load(std::memory_order_relaxed);

		if (!top.compare_exchange_strong(
				t, t + 1,
				std::memory_order_seq_cst,
				std::memory_order_relaxed))
		{
			return nullptr;
		}

		return std::unique_ptr<Job>(raw);
	}

	size_t capacity() const { return mask + 1; }

	/**
	 * @brief Approximate number of jobs currently in the deque.
	 *
	 * The value is approximate because top and bottom are read non-atomically
	 * relative to each other. Safe for diagnostic/heuristic use only.
	 */
	size_t size() const {
		const size_t b = bottom.load(std::memory_order_relaxed);
		const size_t t = top.load(std::memory_order_relaxed);
		return b > t ? b - t : 0;
	}

private:
	const size_t mask;

	alignas(64) std::atomic<size_t> bottom { 0 };
	alignas(64) std::atomic<size_t> top { 0 };

	std::vector<std::atomic<Job*>> buffer;
};

} // namespace Blackthorn::Jobs