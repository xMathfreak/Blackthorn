#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Jobs/JobHandle.h"

namespace Blackthorn::Jobs {

enum class ThreadAffinity : Uint8 {
	Any,        ///< May run on any worker thread or the main thread.
	MainThread  ///< Must only run on the main thread.
};

/**
 * @brief Self-contained unit of work submitted to the JobSystem.
 *
 * Stores the callable inline (up to 64 bytes) to avoid per-job heap
 * allocation. Falls back to a heap-allocated std::function for larger
 * captures.
 *
 * A job optionally holds:
 *   - A dependency handle it waits on before becoming runnable.
 *   - A completion handle it signals when it finishes.
 *   - A thread affinity constraint.
 */
class BLACKTHORN_API Job {
public:
	static constexpr size_t INLINE_SIZE = 64;

	Job() = default;

	/**
	 * @brief Constructs a job from any callable.
	 *
	 * @tparam F Callable type. Must be invocable with no arguments.
	 * @param fn         The work to perform.
	 * @param completion Handle signalled when this job finishes.
	 * @param dependency Handle this job waits on before running.
	 * @param affinity   Thread affinity constraint.
	 */
	template <typename F>
	requires std::invocable<F>
	Job(F&& fn, JobHandlePtr completion = nullptr, JobHandlePtr dependency = nullptr, ThreadAffinity aff = ThreadAffinity::Any)
		: completionHandle(std::move(completion))
		, dependencyHandle(std::move(dependency))
		, affinity(aff)
	{
		using Decayed = std::decay_t<F>;

		if constexpr (sizeof(Decayed) <= INLINE_SIZE &&
					  std::is_trivially_copyable_v<Decayed> &&
					  std::is_trivially_destructible_v<Decayed>
		) {
			new (inlineStorage) Decayed(std::forward<F>(fn));

			invoker = [](void* s) { (*reinterpret_cast<Decayed*>(s))(); };
			mover = nullptr;
			destroyer = nullptr;
		} else if constexpr (sizeof(Decayed) <= INLINE_SIZE) {
			new (inlineStorage) Decayed(std::forward<F>(fn));

			invoker = [](void* s) {
				(*reinterpret_cast<Decayed*>(s))();
			};

			mover = [](void* dst, void* src) {
				new (dst) Decayed(std::move(*reinterpret_cast<Decayed*>(src)));
				reinterpret_cast<Decayed*>(src)->~Decayed();
			};

			destroyer = [](void* s) {
				reinterpret_cast<Decayed*>(s)->~Decayed();
			};
		}
		else
		{
			static_assert(sizeof(std::function<void()>) <= INLINE_SIZE,
				"std::function does not fit in inline storage on this platform");

			new (inlineStorage) std::function<void()>(std::forward<F>(fn));

			invoker = [](void* s) {
				(*reinterpret_cast<std::function<void()>*>(s))();
			};

			mover = [](void* dst, void* src) {
				auto* srcFn = reinterpret_cast<std::function<void()>*>(src);
				new (dst) std::function<void()>(std::move(*srcFn));
				srcFn->~function<void()>();
			};

			destroyer = [](void* s) {
				reinterpret_cast<std::function<void()>*>(s)->~function<void()>();
			};
		}
	}

	~Job() {
		if (destroyer)
			destroyer(inlineStorage);
	}

	Job(const Job&) = delete;
	Job& operator=(const Job&) = delete;

	Job(Job&& other) noexcept
		: invoker(other.invoker)
		, destroyer(other.destroyer)
		, completionHandle(std::move(other.completionHandle))
		, dependencyHandle(std::move(other.dependencyHandle))
		, affinity(other.affinity)
	{
		if (other.invoker) {
			if (other.mover) {
				other.mover(inlineStorage, other.inlineStorage);
			} else {
				memcpy(inlineStorage, other.inlineStorage, INLINE_SIZE);
			}
		}

		other.invoker = nullptr;
		other.mover = nullptr;
		other.destroyer= nullptr;
	}

	Job& operator=(Job&& other) noexcept {
		if (this == &other)
			return *this;

		if (destroyer)
			destroyer(inlineStorage);

		completionHandle = std::move(other.completionHandle);
		dependencyHandle = std::move(other.dependencyHandle);
		affinity  = other.affinity;
		invoker   = other.invoker;
		mover     = other.mover;
		destroyer = other.destroyer;

		if (other.invoker) {
			if (other.mover) {
				other.mover(inlineStorage, other.inlineStorage);
			} else {
				memcpy(inlineStorage, other.inlineStorage, INLINE_SIZE);
			}
		}

		other.invoker  = nullptr;
		other.mover    = nullptr;
		other.destroyer= nullptr;

		return *this;
	}

	/**
	 * @brief Executes the stored callable.
	 */
	void invoke() {
		if (invoker)
			invoker(inlineStorage);
	}

	const JobHandlePtr& getCompletionHandle() const { return completionHandle; }
	const JobHandlePtr& getDependencyHandle() const { return dependencyHandle; }
	ThreadAffinity getAffinity() const { return affinity; }

private:
	alignas(std::max_align_t) std::byte inlineStorage[INLINE_SIZE] {};

	void (*invoker)(void*) = nullptr;
	void (*mover)(void*, void*) = nullptr;
	void (*destroyer)(void*)= nullptr;

	JobHandlePtr completionHandle;
	JobHandlePtr dependencyHandle;
	ThreadAffinity affinity = ThreadAffinity::Any;
};

} // namespace Blackthorn::Jobs