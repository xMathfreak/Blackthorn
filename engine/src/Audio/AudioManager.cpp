#include "Audio/AudioManager.h"

namespace Blackthorn::Audio {

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
	shutdown();
}

bool AudioManager::init(const AudioConfig& cfg) {
	if (initialized) {
		BT_WARN("AudioManager: Already initialized");
		return true;
	}

	if (!audioThread.start(cfg)) {
		BT_ERROR("AudioManager: Failed to start audio thread");
		return false;
	}

	initialized = true;
	return true;
}

void AudioManager::shutdown() {
	if (!initialized)
		return;

	audioThread.stop();
	initialized = false;
}

AudioHandle AudioManager::play(
	AudioClip& clip,
	const PlayOptions& options
) {
	AudioHandle handle = {nextAudioHandle.fetch_add(1, std::memory_order::relaxed)};
	PlayCommand cmd{};

	cmd.handle = handle;
	cmd.clipSource = &clip;

	cmd.volume = options.volume.sample(rng);
	cmd.pitch = options.pitch.sample(rng);

	cmd.category = options.category;
	cmd.priority = options.priority;

	cmd.loop = options.loop;

	switch (options.mode) {
		case PlaybackMode::Auto:
			cmd.stream = clip.hasPCM()
				? false
				: clip.estimatedBytes() > config.streamingThreshold;
			break;
		case PlaybackMode::PCM:
			cmd.stream = false;
			break;
		case PlaybackMode::Stream:
			cmd.stream = true;
			break;
	}

	if (options.spatial.has_value()) {
		cmd.spatial = true;
		cmd.position = options.spatial->position;
		cmd.minDistance = options.spatial->minDistance;
		cmd.maxDistance = options.spatial->maxDistance;
	}

	audioThread.enqueue(cmd);

	return handle;
}

void AudioManager::pause(AudioHandle handle) {
	if (handle.isValid())
		audioThread.enqueue(PauseCommand{handle});
}

void AudioManager::resume(AudioHandle handle) {
	if (handle.isValid())
		audioThread.enqueue(ResumeCommand{handle});
}

void AudioManager::stop(AudioHandle handle) {
	if (handle.isValid())
		audioThread.enqueue(StopCommand{handle});
}

void AudioManager::setVolume(AudioHandle handle, float volume) {
	if (handle.isValid())
		audioThread.enqueue(SetVolumeCommand{handle, volume});
}

void AudioManager::setCategoryVolume(AudioCategory category, float volume) {
	audioThread.enqueue(SetCategoryVolumeCommand{category, volume});
}

void AudioManager::setPitch(AudioHandle handle, float pitch) {
	if (handle.isValid())
		audioThread.enqueue(SetPitchCommand{handle, pitch});
}

void AudioManager::stopAll() {
	audioThread.enqueue(StopAllCommand{});
}

void AudioManager::setPosition(
	AudioHandle handle,
	const glm::vec3& position
) {
	if (handle.isValid())
		audioThread.enqueue(SetPositionCommand{ handle, position });
}

void AudioManager::setListenerTransform(
	const glm::vec3& position,
	const glm::vec3& forward,
	const glm::vec3& up,
	const glm::vec3& velocity
) {
	audioThread.enqueue(ListenerTransformCommand{
		position, forward, up, velocity
	});
}

void AudioManager::update() {
	audioThread.views().acquire();
}

const VoiceView& AudioManager::getView(AudioHandle handle) {
	return audioThread.views().query(handle);
}

bool AudioManager::isValid(AudioHandle handle) {
	if (!handle.isValid())
		return false;
	return getView(handle).state != PlaybackState::Inactive;
}

bool AudioManager::isPlaying(AudioHandle handle) {
	return getView(handle).state == PlaybackState::Playing;
}

bool AudioManager::isPaused(AudioHandle handle) {
	return getView(handle).state == PlaybackState::Paused;
}

bool AudioManager::isStopped(AudioHandle handle) {
	const PlaybackState s = getView(handle).state;
	return s == PlaybackState::Stopped || s == PlaybackState::Inactive;
}

bool AudioManager::isLooping(AudioHandle handle) {
	return getView(handle).flags.test(VoiceFlagBit::Looping);
}

bool AudioManager::isStreaming(AudioHandle handle) {
	return getView(handle).flags.test(VoiceFlagBit::Streaming);
}

bool AudioManager::isResident(AudioHandle handle) {
	const VoiceView& s = getView(handle);

	return s.state != PlaybackState::Inactive &&
		!s.flags.test(VoiceFlagBit::Streaming);
}

float AudioManager::getDuration(AudioHandle handle) {
	return getView(handle).duration;
}

float AudioManager::getPlaybackTime(AudioHandle handle) {
	return getView(handle).playbackPosition;
}

float AudioManager::getNormalizedTime(AudioHandle handle) {
	const VoiceView& snap = getView(handle);

	if (snap.duration <= 0.0f)
		return 0.0f;

	return std::clamp(snap.playbackPosition / snap.duration, 0.0f, 1.0f);
}

float AudioManager::getRemainingTime(AudioHandle handle) {
	const VoiceView& snap = getView(handle);
	return std::max(snap.duration - snap.playbackPosition, 0.0f);
}

float AudioManager::getVolume(AudioHandle handle) {
	return getView(handle).volume;
}

float AudioManager::getPitch(AudioHandle handle) {
	const VoiceView& snap = getView(handle);
	return snap.state != PlaybackState::Inactive ? snap.pitch : 1.0f;
}

} // namespace Blackthorn::Audio
