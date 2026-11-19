#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include "Blackthorn/Assets/AssetResolver.hpp"
#include "Blackthorn/Audio/AudioConfig.hpp"
#include "Blackthorn/Audio/Backend/AudioContext.hpp"
#include "Blackthorn/Audio/Backend/AudioDevice.hpp"
#include "Blackthorn/Audio/Commands/AudioCommand.hpp"
#include "Blackthorn/Audio/Commands/AudioCommandQueue.hpp"
#include "Blackthorn/Audio/Core/StreamingThread.hpp"
#include "Blackthorn/Audio/Device/IDeviceNotifier.hpp"
#include "Blackthorn/Audio/Playback/VoicePool.hpp"
#include "Blackthorn/Audio/Playback/VoiceViewPool.hpp"
#include "Blackthorn/Audio/Playback/VoiceSnapshot.hpp"
#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Audio {

inline float computeGain(
	float baseVolume,
	AudioCategory category,
	const std::array<float, AUDIO_CATEGORY_COUNT>& categoryVolumes
) {
	const float catVol = categoryVolumes[static_cast<size_t>(category)];
	const float masterVol =
		categoryVolumes[static_cast<size_t>(AudioCategory::Master)];
	return baseVolume * catVol * masterVol;
}

enum class AudioThreadState : U8 {
	Running,
	Stopped,
	Reconnecting,
	DeviceLost,
	Migrating
};

class BLACKTHORN_API AudioThread {
public:
	AudioThread();
	~AudioThread();

	AudioThread(const AudioThread&) = delete;
	AudioThread& operator=(const AudioThread&) = delete;

	bool start(const AudioConfig& cfg);
	void stop();

	[[nodiscard]] bool isRunning() const noexcept {
		return state.load() != AudioThreadState::Stopped;
	}

	void enqueue(AudioCommand command);

	[[nodiscard]]
	VoiceViewPool& views() const noexcept;

	void setAssetResolver(const Assets::AssetResolver& resolver) {
		assetResolver = &resolver;
	}

private:
	void threadLoop();

	void tickStreaming();
	void tickViews();

	void processResidentPlayback(
		Voice& voice,
		AudioClip& clip,
		float seekSeconds = 0.0f
	);

	void processStreamingPlayback(
		Voice& voice,
		const AudioClip& clip,
		U64 startFrame
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

	void processCommand(AudioCommand cmd);
	void drainStreamResults();

	[[nodiscard]]
	bool shouldRestoreVoice(const Voice& voice) const noexcept;

	void restoreVoices();
	void attemptReconnect(AudioThreadState returnStateOnFailure);
	void enterRecovery(AudioThreadState reason);
	[[nodiscard]] bool defaultDeviceChanged() const noexcept;
	void tickDeviceHealth();
	void updatePlaybackTimes();
	void performStreamingSeek(Voice& voice, float seconds);

private:
	AudioCommandQueue commandQueue;

	StreamDecodedQueue streamResultQueue;

	std::unique_ptr<VoicePool> voicePool = nullptr;
	std::unique_ptr<VoiceViewPool> viewPool = nullptr;

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

	const Assets::AssetResolver* assetResolver = nullptr;
	AudioConfig config;

private:
	void process(PlayCommand&& cmd);
	void process(StopCommand cmd);
	void process(PauseCommand cmd);
	void process(ResumeCommand cmd);
	void process(SetVolumeCommand cmd);
	void process(SetPitchCommand cmd);
	void process(SetPositionCommand cmd);
	void process(SetCategoryVolumeCommand cmd);
	void process(ListenerTransformCommand cmd);
	void process(StopAllCommand cmd);
	void process(StreamBufferReadyCommand&& cmd);
	void process(SeekCommand cmd);

private:
	std::vector<VoiceSnapshot> voiceSnapshots;

	std::chrono::steady_clock::time_point outageStartTime;
	std::chrono::steady_clock::time_point nextRetryTime;
	size_t backoffIndex = 0;

	static constexpr std::array<int, 6> kLossBackoffMs {
		250, 500, 1000, 1500, 2000, 3000
	};

	static constexpr std::array<int, 4> kMigrationBackoffMs {
		100, 200, 500, 1000
	};

	std::atomic<bool> pendingMigrationHint { false };

	static constexpr float kMinRemainingTime = 0.5f;
};

} // namespace Blackthorn::Audio