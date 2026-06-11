#pragma once

#include <atomic>
#include <condition_variable>
#include <future>
#include <thread>

#include "Audio/Commands/AudioCommandQueue.h"
#include "Audio/Core/StreamingJob.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

/**
 * @brief Worker thread that decodes streaming audio chunks off the audio
 *        thread.
 *
 * @c StreamingThread owns the decode loop. It pops @c StreamingJob requests
 * from @c jobQueue (pushed by the audio thread via @c submitJob()), decodes
 * one chunk of PCM via @c IStreamDecoder::readFrames(), and pushes a
 * @c StreamBufferReadyCommand onto the audio thread's result queue, then
 * notifies the audio thread's condition variable so it wakes immediately.
 *
 * @section wakeup Wake-up strategy
 * The thread sleeps on @c wakeCv until either:
 * - @c submitJob() pushes a job and calls @c notify_one(), or
 * - @c stop() sets @c running = false and calls @c notify_one().
 *
 * There is no periodic timeout — the streaming thread has no maintenance
 * work to do between jobs. This eliminates CPU spinning entirely.
 *
 * @section result_notification Result notification
 * After pushing a @c StreamBufferReadyCommand, the streaming thread notifies
 * the audio thread's condition variable (passed via @c start()) so the audio
 * thread wakes immediately to process the new chunk rather than waiting up
 * to 5 ms for its next scheduled tick.
 *
 * @section ownership Decoder ownership
 * The @c decoder pointer inside @c StreamingJob is non-owning. The decoder
 * is owned by @c StreamingVoiceState, which lives inside the @c Voice, which
 * is held by @c VoicePool. The audio thread never touches the decoder while
 * a job is in flight; the streaming thread never touches it after pushing the
 * result command. No synchronization beyond the SPSC queues is required.
 */
class BLACKTHORN_API StreamingThread {
public:
	/**
	 * @brief PCM frames decoded per job.
	 *
	 * 4096 frames × 2 channels × 2 bytes/sample = 16 KiB per chunk maximum.
	 */
	static constexpr size_t DECODE_FRAMES_PER_CHUNK = 4096;

	StreamingThread() = default;
	~StreamingThread() { stop(); }

	StreamingThread(const StreamingThread&) = delete;
	StreamingThread& operator=(const StreamingThread&) = delete;

	StreamingThread(StreamingThread&&) = delete;
	StreamingThread& operator=(StreamingThread&&) = delete;

	/**
	 * @brief Starts the streaming worker thread.
	 *
	 * @param resultQueue  The audio thread's @c StreamDecodedQueue. Must
	 *                     outlive this @c StreamingThread.
	 * @param audioWakeCv  The audio thread's condition variable. Notified
	 *                     after each result is pushed so the audio thread
	 *                     wakes immediately.
	 * @param audioWakeMtx The mutex paired with @p audioWakeCv.
	 * @return             true on success.
	 */
	bool start(
		StreamDecodedQueue& queue,
		std::condition_variable& audioCv,
		std::mutex& audioWakeMtx
	);

	/**
	 * @brief Signals the thread to stop and blocks until it exits.
	 *
	 * Any job currently being decoded is completed before the thread exits.
	 * No new jobs are accepted after this call returns.
	 */
	void stop();

	/** @brief Returns true if the thread is running. */
	[[nodiscard]]
	bool isRunning() const noexcept {
		return running.load(std::memory_order::relaxed);
	}

	/**
	 * @brief Enqueues a decode job.
	 *
	 * Must only be called from the audio thread (SPSC single-producer rule).
	 *
	 * @param job Decode descriptor. @c job.decoder must not be null.
	 */
	void submitJob(StreamingJob&& job);

private:
	void threadLoop();

	/**
	 * @brief Decodes one chunk from @p job and pushes a
	 *        @c StreamBufferReadyCommand to @c resultQueue.
	 */
	void processJob(const StreamingJob& job);

private:
	StreamingJobQueue jobQueue;

	/// Pointers to the audio thread's CV and queue — set by start(), valid
	/// for the lifetime of the thread.
	std::condition_variable* audioWakeCv = nullptr;
	std::mutex* audioWakeMutex = nullptr;
	StreamDecodedQueue* resultQueue = nullptr;

	std::thread thread;
	std::atomic<bool> running { false };

	std::mutex wakeMutex;
	std::condition_variable wakeCv;

	std::promise<void> readyPromise;
};

} // namespace Blackthorn::Audio