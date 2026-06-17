#include "Audio/Core/StreamingThread.h"

#include "Audio/Commands/AudioCommand.h"
#include "Debug/Logger.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Audio {

bool StreamingThread::start(
	StreamDecodedQueue& queue,
	std::condition_variable& audioCv,
	std::mutex& audioWakeMtx
) {
	if (running.load(std::memory_order::relaxed)) {
		BT_WARN("StreamingThread: already running");
		return false;
	}

	this->resultQueue = &queue;
	this->audioWakeCv = &audioCv;
	this->audioWakeMutex = &audioWakeMtx;

	readyPromise = std::promise<void>();
	std::future<void> ready = readyPromise.get_future();

	running.store(true, std::memory_order::release);
	thread = std::thread([this] { threadLoop(); });

	ready.wait();
	return true;
}

void StreamingThread::stop() {
	if (!running.exchange(false, std::memory_order::acq_rel))
		return;

	wakeCv.notify_one();

	if (thread.joinable())
		thread.join();
}

void StreamingThread::submitJob(StreamingJob&& job) {
	if (!jobQueue.push(std::move(job)))
		BT_WARN(
			"StreamingThread: job queue full, dropping job for handle {}",
			job.handle.id
		);

	wakeCv.notify_one();
}

void StreamingThread::threadLoop() {
	Threads::ThreadRegistry::instance().registerCurrent("AudioStreaming");
	BT_LOG("StreamingThread: decode thread started");

	readyPromise.set_value();

	while (running.load(std::memory_order::relaxed)) {
		{
			std::unique_lock lock(wakeMutex);
			wakeCv.wait(lock, [this] {
				return !jobQueue.empty()
					|| !running.load(std::memory_order::relaxed);
			});
		}

		StreamingJob j;
		while (jobQueue.pop(j))
			processJob(j);
	}

	StreamingJob j;
	while (jobQueue.pop(j))
		processJob(j);

	BT_LOG("StreamingThread: decode thread stopped");
	Threads::ThreadRegistry::instance().unregisterCurrent();
}

void StreamingThread::processJob(const StreamingJob& job) {
	if (!job.decoder) {
		BT_WARN("StreamingThread: job has null decoder (handle {})",
			job.handle.id);
		return;
	}

	const size_t maxSamples = job.frameCount * job.channels;

	if (scratchBuffer.size() < maxSamples)
		scratchBuffer.resize(maxSamples);

	const size_t framesRead =
		job.decoder->readFrames(scratchBuffer.data(), job.frameCount);

	const size_t samplesRead = framesRead * job.channels;
	const bool shortRead = (framesRead < job.frameCount);
	const bool endOfStream = shortRead && !job.looping;

	StreamBufferReadyCommand cmd;
	cmd.handle = job.handle;
	cmd.channels = job.channels;
	cmd.sampleRate = job.sampleRate;
	cmd.endOfStream = endOfStream;

	cmd.samples.assign(scratchBuffer.data(), scratchBuffer.data() + samplesRead);

	if (!resultQueue->push(AudioCommand{ std::move(cmd) })) {
		BT_WARN(
			"StreamingThread: result queue full, decoded chunk for "
			"handle {} dropped", job.handle.id
		);
		return;
	}

	audioWakeCv->notify_one();
}

} // namespace Blackthorn::Audio