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

	assert(
		mainTail.load(std::memory_order_acquire)->next.load(std::memory_order_acquire) == nullptr &&
		"JobSystem destroyed with pending main-thread jobs — call flushMainThread() before shutdown"
	);

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
	} else {
		size_t idx = nextWorker.fetch_add(1, std::memory_order_relaxed) % queues.size();

		if (!queues[idx]->push(std::make_unique<Job>(std::move(job)))) {
			BT_ERROR(
				"JobSystem: worker queue {} full — job dropped, "
				"its handle will never complete", idx
			);
		}
	}
}

void JobSystem::submit(Job job) {
	auto enqueueCallback = [this](std::function<void()> fn, bool) {
		enqueueReady(Job(std::move(fn)));
	};

	const auto& dep = job.getDependencyHandle();

	if (dep && !dep->isComplete()) {
		const bool mt = job.getAffinity() == ThreadAffinity::MainThread;
		auto sharedJob = std::make_shared<Job>(std::move(job));

		dep->addContinuation(
			[this, sharedJob, enqueueCallback] {
				auto enqFn = enqueueCallback;
				enqueueReady(Job(
					[sharedJob, enqFn] {
						sharedJob->invoke();
						if (const auto& h = sharedJob->getCompletionHandle())
							h->signal(enqFn);
					},
					nullptr,
					nullptr,
					sharedJob->getAffinity()
				));
				++activeJobs;
			},
			mt,
			enqueueCallback
		);
		return;
	}

	++activeJobs;

	auto enqFn = enqueueCallback;
	const ThreadAffinity affinity = job.getAffinity();
	auto sharedJob = std::make_shared<Job>(std::move(job));

	enqueueReady(Job(
		[this, sharedJob, enqFn] {
			sharedJob->invoke();
			if (const auto& h = sharedJob->getCompletionHandle())
				h->signal(enqFn);
			--activeJobs;
		},
		nullptr,
		nullptr,
		affinity
	));
}

bool JobSystem::executeOne(bool mainThreadOnly) {
	if (mainThreadOnly || getWorkerIndex() == -1) {
		MainThreadNode* tail = mainTail.load(std::memory_order_acquire);
		MainThreadNode* next = tail->next.load(std::memory_order_acquire);

		if (next) {
			mainTail.store(next, std::memory_order_release);
			next->job.invoke();
			delete tail;
			return true;
		}

		if (mainThreadOnly)
			return false;
	}

	if (getWorkerIndex() >= 0) {
		if (auto job = queues[getWorkerIndex()]->pop()) {
			job->invoke();
			return true;
		}
	}

	const size_t n = queues.size();
	const size_t start = getWorkerIndex() >= 0
		? static_cast<size_t>(getWorkerIndex())
		: 0;

	for (size_t i = 1; i <= n; ++i) {
		if (auto job = queues[(start + i) % n]->steal()) {
			job->invoke();
			return true;
		}
	}

	return false;
}

void JobSystem::flushMainThread() {
	while (executeOne(true)) {}
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