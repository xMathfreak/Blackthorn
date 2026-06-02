#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include "Audio/Backend/AudioContext.h"
#include "Audio/Backend/AudioDevice.h"
#include "Audio/Commands/AudioCommand.h"
#include "Audio/Commands/AudioCommandQueue.h"
#include "Audio/Core/StreamingThread.h"
#include "Audio/Device/IDeviceNotifier.h"
#include "Audio/Playback/VoicePool.h"
#include "Audio/Playback/VoiceSnapshot.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

enum class AudioThreadState : U8 {
	Running,
	Stopped,
	Reconnecting,
	DeviceLost
};

class BLACKTHORN_API AudioThread {
public:
	AudioThread();
	~AudioThread();

	AudioThread(const AudioThread&) = delete;
	AudioThread& operator=(const AudioThread&) = delete;

	bool start();
	void stop();

	bool isRunning() const noexcept {
		return state.load() != AudioThreadState::Stopped;
	}

	void enqueue(AudioCommand command);

private:
	void threadLoop();

	void tickStreaming();

	void processResidentPlayback(
		Voice& voice,
		const AudioClip& clip,
		float seekSeconds = 0.0f
	);

	void processStreamingPlayback(
		Voice& voice,
		const AudioClip& clip,
		U64 startTick
	);

	size_t prefillBuffer(
		StreamingVoiceState& sstate,
		ALuint alBuffer,
		bool looping
	);

	void submitStreamingJob(
		Voice& voice,
		StreamingVoiceState& sstate,
		bool previousChunkWasShort
	);

	void processCommand(const AudioCommand& cmd);
	void drainStreamResults();

	[[nodiscard]]
	bool shouldRestoreVoice(const Voice& voice) const noexcept;

	void restoreVoices();
	void attemptReconnect();
	void enterDeviceLost();
	void tickDeviceHealth();
	void updatePlaybackTimes();

private:
	AudioCommandQueue commandQueue;

	StreamDecodedQueue streamResultQueue;

	std::unique_ptr<VoicePool> voicePool = nullptr;

	std::array<float, AUDIO_CATEGORY_COUNT> categoryVolumes;

	std::optional<AudioContext> context;
	std::optional<AudioDevice>  device;
	std::unique_ptr<IDeviceNotifier> deviceNotifier;

	StreamingThread streamingThread;

	std::atomic<U64> tick { 0 };
	std::atomic<AudioThreadState> state { AudioThreadState::Stopped };

	std::thread thread;
	std::condition_variable wakeCv;
	std::mutex wakeMutex;

	U64 nextHandleId {1};

private:
	void process(const PlayCommand& cmd);
	void process(const StopCommand& cmd);
	void process(const PauseCommand& cmd);
	void process(const ResumeCommand& cmd);
	void process(const SetVolumeCommand& cmd);
	void process(const SetPitchCommand& cmd);
	void process(const SetPositionCommand& cmd);
	void process(const SetCategoryVolumeCommand& cmd);
	void process(const ListenerTransformCommand& cmd);
	void process(const StopAllCommand& cmd);
	void process(const StreamBufferReadyCommand& cmd);

private:
	std::vector<VoiceSnapshot> voiceSnapshots;

	std::chrono::steady_clock::time_point lossTime;
	std::chrono::steady_clock::time_point nextRetryTime;
	size_t backoffIndex = 0;

	static constexpr std::array<int, 6> kBackoffMs {
		250, 500, 100, 1500, 3000
	};

	static constexpr float kMinRestoreDuration = 2.0f;
	static constexpr float kMinRemainingTime = 0.5f;
};

} // namespace Blackthorn::Audio