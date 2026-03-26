#include "Jobs/JobSystem.h"

#include <cassert>
#include <thread>

#include "Debug/Logger.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Jobs {

thread_local int workerIndex = -1;

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
	for (auto& t : workers)
		t.join();

	flushMainThread();

	delete mainTail.load();
}

JobHandlePtr JobSystem::createHandle() {
	return JobHandle::create();
}

void JobSystem::enqueueReady(Job&& job) {
	if (job.getAffinity() == ThreadAffinity::MainThread) {
		auto* node = new MainThreadNode(std::move(job));
		MainThreadNode* prev = mainHead.exchange(node, std::memory_order_acq_rel);
		prev->next.store(node, std::memory_order_release);

		++activeJobs;
	} else {
		size_t idx = nextWorker.fetch_add(1, std::memory_order_relaxed) % queues.size();
		if (!queues[idx]->push(std::make_unique<Job>(std::move(job)))) {
			BT_ERROR("JobSystem: worker queue {} full — job dropped, its handle will never complete", idx);
		} else {
			++activeJobs;
		}
	}
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

	if (mainThreadOnly || workerIndex == -1) {
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

	if (workerIndex >= 0) {
		if (auto job = queues[workerIndex]->pop()) {
			runJob(*job);
			return true;
		}
	}

	const size_t n = queues.size();
	const size_t start = workerIndex >= 0 ? static_cast<size_t>(workerIndex) : 0;

	for (size_t i = 1; i <= n; ++i) {
		if (auto job = queues[(start + i) % n]->steal()) {
			runJob(*job);
			return true;
		}
	}

	return false;
}

void JobSystem::flushMainThread() {
	while (activeJobs.load(std::memory_order_acquire) > 0) {
		while (executeOne(true)) {}
		std::this_thread::yield();
	}
}

void JobSystem::wait(const JobHandlePtr& handle) {
	while (!handle->isComplete()) {
		if (!executeOne(false))
			std::this_thread::yield();
	}

	if (getWorkerIndex() == -1)
		flushMainThread();
}

void JobSystem::workerLoop(size_t idx) {
	workerIndex = static_cast<int>(idx);

	Threads::ThreadRegistry::instance().registerCurrent(
		"JobWorker-" + std::to_string(idx));

	while (!shutdown.load(std::memory_order_relaxed)) {
		if (!executeOne(false))
			std::this_thread::yield();
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