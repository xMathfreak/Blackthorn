#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "Core/Export.h"

namespace Blackthorn::Jobs {

/**
 * @brief Shared completion token for one or more jobs.
 *
 * A handle tracks how many jobs are still contributing to it via an atomic
 * pending count. When the count reaches zero the handle is complete and all
 * registered continuations are enqueued automatically.
 *
 * Handles are always heap-allocated and reference-counted so they can be
 * shared freely between the submitter, the jobs themselves, and any
 * downstream dependents without lifetime concerns.
 *
 * @note All methods are safe to call from any thread.
 */
class BLACKTHORN_API JobHandle {
public:
	/**
	 * @brief Creates a handle with an initial pending count of 1.
	 *
	 * Increment this before submitting each additional job that contributes
	 * to the handle, via addPending().
	 */
	static std::shared_ptr<JobHandle> create();

	/**
	 * @brief Creates a completed handle.
	 */
	static std::shared_ptr<JobHandle> createComplete();

	/**
	 * @brief Increments the pending count.
	 *
	 * Call once per job that should contribute to this handle, before
	 * submitting that job. Must not be called after the handle is complete.
	 */
	void addPending(int count = 1);

	/**
	 * @brief Signals one unit of completion.
	 *
	 * Called internally by the JobSystem when a contributing job finishes.
	 * When the pending count reaches zero, all registered continuations are
	 * enqueued via the provided enqueue callback and the output slot is
	 * published.
	 *
	 * @param enqueue Callable used to enqueue ready continuations.
	 */
	void signal(const std::function<void(std::function<void()>, bool)>& enqueue);

	/**
	 * @brief Returns true if all contributing jobs have completed.
	 */
	bool isComplete() const;

	/**
	 * @brief Registers a continuation to be enqueued when this handle completes.
	 *
	 * If the handle is already complete when this is called, the continuation
	 * is enqueued immediately via the provided enqueue callback. This avoids
	 * a race between completion and registration.
	 *
	 * @param fn       The continuation callable.
	 * @param mainThread Whether the continuation must run on the main thread.
	 * @param enqueue  Callable used to enqueue if already complete.
	 */
	void addContinuation(
		std::function<void()> fn,
		bool mainThread,
		const std::function<void(std::function<void()>, bool)>& enqueue
	);

	/**
	 * @brief Stores a typed output pointer produced by a contributing job.
	 *
	 * The pointer must remain valid until all downstream consumers have
	 * finished reading it. Ownership is not transferred - the job lambda
	 * is responsible for the buffer lifetime.
	 *
	 * @tparam T Output type.
	 * @param ptr Pointer to the output data.
	 */
	template <typename T>
	void setOutput(T* ptr) {
		output.store(static_cast<void*>(ptr), std::memory_order::release);
	}

	/**
	 * @brief Retrieves the typed output pointer.
	 *
	 * Only safe to call after the handle is complete (i.e. inside a job
	 * that declared this handle as its dependency).
	 *
	 * @tparam T Output type.
	 * @return Pointer to the output data, or nullptr if not set.
	 */
	template <typename T>
	T* getOutput() const {
		return static_cast<T*>(output.load(std::memory_order::acquire));
	}

private:
	JobHandle() = default;

	struct Continuation {
		std::function<void()> fn;
		bool mainThread = false;
		Continuation* next = nullptr;
	};

	std::atomic<int> pendingCount { 1 };
	std::atomic<void*> output { nullptr };

	mutable std::mutex continuationMutex;

	// Lock-free intrusive singly-linked list of continuations.
	// Head == nullptr means empty. Head == COMPLETE_SENTINEL means the
	// handle has already fired - used to handle the race between
	// addContinuation and signal.
	std::atomic<Continuation*> continuationHead { nullptr };

	static Continuation* const COMPLETE_SENTINEL;
};

using JobHandlePtr = std::shared_ptr<JobHandle>;

} // namespace Blackthorn::Jobs