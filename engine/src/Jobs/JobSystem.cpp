#include "Jobs/JobSystem.h"

#include <cassert>
#include <thread>

#include "Debug/Logger.h"
#include "Threads/ThreadRegistry.h"

namespace {
	thread_local int workerIndex = -1;

	thread_local Uint32 stealRNG = 0x9E3779B9u;

	inline Uint32 nextRand() {
		stealRNG ^= stealRNG << 13;
		stealRNG ^= stealRNG >> 17;
		stealRNG ^= stealRNG << 5;

		return stealRNG;
	};

}

namespace Blackthorn::Jobs {

JobSystem::JobSystem(size_t workerCount) {
	if (workerCount == 0) {
		const size_t hw = std::thread::hardware_concurrency();
		workerCount = hw > 1 ? hw - 1 : 1;
	}

	auto* dummy = new MainThreadNode();
	mainHead.store(dummy, std::memory_order_relaxed);
	mainTail.store(dummy, std::memory_order_relaxed);

	queues.reserve(workerCount);
	for (size_t i = 0; i < workerCount; ++i)
		queues.push_back(std::make_unique<JobQueue>());

	workers.reserve(workerCount);
	for (size_t i = 0; i < workerCount; ++i) {
		workers.emplace_back([this, i] {
			workerLoop(i);
		});
	}

	BT_DEBUG("JobSystem initialised ({} worker thread{})",
		workerCount, workerCount == 1 ? "" : "s");
}

JobSystem::~JobSystem() {
	shutdown.store(true, std::memory_order_release);

	wakeCondition.notify_all();

	for (auto& t : workers)
		t.join();

	flushMainThread();
	delete mainTail.load();
}

JobHandlePtr JobSystem::createHandle() {
	return JobHandle::create();
}

void JobSystem::submit(Job job) {
	JobHandlePtr dep = job.getDependencyHandle();

	if (dep && !dep->isComplete()) {
		const bool mt = job.getAffinity() == ThreadAffinity::MainThread;
		auto sharedJob = std::make_shared<Job>(std::move(job));
		auto completionHdl = sharedJob->getCompletionHandle();

		dep->addContinuation(
			[this, sharedJob, completionHdl] {
				sharedJob->invoke();
				if (completionHdl) {
					completionHdl->signal([this](std::function<void()> fn, bool isMt) {
						enqueueReady(Job(std::move(fn), nullptr, nullptr,
							isMt ? ThreadAffinity::MainThread : ThreadAffinity::Any));
					});
				}
			},
			mt,
			[this](std::function<void()> fn, bool isMt) {
				enqueueReady(Job(std::move(fn), nullptr, nullptr,
					isMt ? ThreadAffinity::MainThread : ThreadAffinity::Any));
			}
		);
		return;
	}

	enqueueReady(std::move(job));
}

void JobSystem::flushMainThread() {
	while (executeOne(true)) {}
}

void JobSystem::wait(const JobHandlePtr& handle) {
	while (!handle->isComplete()) {
		if (!executeOne(false))
			std::this_thread::yield();
	}
}

void JobSystem::enqueueReady(Job&& job) {
	if (job.getAffinity() == ThreadAffinity::MainThread) {
		++activeJobs;
		auto* node = new MainThreadNode(std::move(job));
		MainThreadNode* prev = mainHead.exchange(node, std::memory_order_acq_rel);
		prev->next.store(node, std::memory_order_release);
	} else {
		++activeJobs;
		++pendingWork;

		int localIdx = getWorkerIndex();
		size_t idx = (localIdx >= 0)
			? static_cast<size_t>(localIdx)
			: nextWorker.fetch_add(1, std::memory_order_relaxed) % queues.size();

		if (!queues[idx]->push(std::make_unique<Job>(std::move(job)))) {
			--activeJobs;
			--pendingWork;

			BT_ERROR("JobSystem: worker queue {} full, Job dropped", idx);
			return;
		}

		wakeCondition.notify_one();
	}
}

bool JobSystem::executeOne(bool mainThreadOnly) {
	auto runJob = [this](Job& job) {
		job.invoke();

		if (const auto& h = job.getCompletionHandle()) {
			h->signal([this](std::function<void()> fn, bool isMt) {
				enqueueReady(Job(std::move(fn), nullptr, nullptr,
					isMt ? ThreadAffinity::MainThread : ThreadAffinity::Any));
			});
		}

		--activeJobs;
	};

	if (mainThreadOnly || getWorkerIndex() == -1) {
		MainThreadNode* tail = mainTail.load(std::memory_order_acquire);
		MainThreadNode* next = tail->next.load(std::memory_order_acquire);

		if (next) {
			mainTail.store(next, std::memory_order_release);
			runJob(next->job);
			delete tail;
			return true;
		}

		if (mainThreadOnly)
			return false;
	}

	if (getWorkerIndex() >= 0) {
		if (auto job = queues[getWorkerIndex()]->pop()) {
			--pendingWork;
			runJob(*job);
			return true;
		}
	}

	const size_t n = queues.size();

	for (size_t i = 0; i < n; ++i) {
		size_t target = nextRand() % n;

		if (target == static_cast<size_t>(getWorkerIndex()))
			continue;

		if (auto job = queues[target]->steal()) {
			--pendingWork;
			runJob(*job);
			return true;
		}
	}

	return false;
}

void JobSystem::workerLoop(size_t idx) {
	setWorkerIndex(idx);
	Threads::ThreadRegistry::instance().registerCurrent(
		"JobWorker-" + std::to_string(idx));

	while (true) {
		if (executeOne(false))
			continue;

		{
			std::unique_lock<std::mutex> lock(wakeMutex);
			wakeCondition.wait(lock, [this] {
				return pendingWork.load(std::memory_order_relaxed) > 0
					|| shutdown.load(std::memory_order_relaxed);
			});
		}

		if (shutdown.load(std::memory_order_relaxed))
			break;
	}

	while (executeOne(false)) {}

	Threads::ThreadRegistry::instance().unregisterCurrent();
}

int JobSystem::getWorkerIndex() {
	return workerIndex;
}

void JobSystem::setWorkerIndex(int idx) {
	workerIndex = idx;
}

} // namespace Blackthorn::Jobs