#pragma once

#include <optional>

#include "Audio/AudioConfig.h"
#include "Audio/Core/AudioThread.h"
#include "Core/Export.h"
#include "Math/NumericRange.h"
#include "Math/Random.h"

namespace Blackthorn::Audio {

enum class PlaybackMode : U8 {
	Auto,
	PCM,
	Stream,
	Resident = PCM
};

struct BLACKTHORN_API SpatialOptions {
	glm::vec3 position{0.0f};

	float minDistance = 1.0f;
	float maxDistance = 50.0f;
};

struct BLACKTHORN_API PlayOptions {
	Math::FloatRange volume = 1.0f;
	Math::FloatRange pitch = 1.0f;

	AudioCategory category = AudioCategory::SFX;
	int priority = 0;

	bool loop = false;
	PlaybackMode mode = PlaybackMode::Auto;

	std::optional<SpatialOptions> spatial = std::nullopt;
};

/**
 * @class AudioManager
 * @brief Brief description of AudioManager.
 *
 * Detailed description of what this class does.
 */
class BLACKTHORN_API AudioManager {
public:
	/** @brief Default constructor. */
	AudioManager();

	/** @brief Destructor. */
	~AudioManager();

	bool init(const AudioConfig& cfg = AudioConfig{});
	void shutdown();

	AudioHandle play(AudioClip& clip, const PlayOptions& options = {});
	void pause(AudioHandle handle);
	void resume(AudioHandle handle);
	void stop(AudioHandle handle);
	void setVolume(AudioHandle handle, float volume);
	void setCategoryVolume(AudioCategory category, float volume);
	void setPitch(AudioHandle handle, float pitch);
	void stopAll();
	void setPosition(AudioHandle handle, const glm::vec3& position);
	void setListenerTransform(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up, const glm::vec3& velocity);

	/**
	 * @brief Acquires the latest published snapshot buffer.
	 *
	 * Must be called before any query method to ensure a consistent read.
	 * Call once per frame (or per batch of queries) rather than before
	 * each individual query.
	 */
	void update();

	/**
	 * @brief Returns true if a voice is active (was issued and not released).
	 *
	 * A handle is valid from the moment @c play() returns it until the voice
	 * finishes or is stopped. Note that a handle may become invalid between
	 * the last @c refreshSnapshots() and the current query if the voice
	 * finished in the last tick.
	 */
	[[nodiscard]] bool isValid(AudioHandle handle);

	/** @brief True if the voice is currently producing audio. */
	[[nodiscard]] bool isPlaying(AudioHandle handle);

	/** @brief True if the voice is paused. */
	[[nodiscard]] bool isPaused(AudioHandle handle);

	/** @brief True if the voice has stopped (finished or explicitly stopped). */
	[[nodiscard]] bool isStopped(AudioHandle handle);

	/** @brief True if the voice is set to loop continuously. */
	[[nodiscard]] bool isLooping(AudioHandle handle);

	/** @brief True if the clip backing this voice is streaming from disk. */
	[[nodiscard]] bool isStreaming(AudioHandle handle);

	/** @brief True if the clip backing this voice is fully resident in memory. */
	[[nodiscard]] bool isResident(AudioHandle handle);

	/**
	 * @brief Returns the clip's total duration in seconds.
	 *
	 * Returns 0 if the handle is invalid or the voice is inactive.
	 */
	[[nodiscard]] float getDuration(AudioHandle handle);

	/**
	 * @brief Returns the current playback position within the clip, in seconds.
	 *
	 * Wraps at the clip boundary for looping voices. For resident clips,
	 * sourced from @c AL_SEC_OFFSET. For streaming clips, accumulated from
	 * decoded frame counts.
	 *
	 * Returns 0 for inactive handles.
	 */
	[[nodiscard]] float getPlaybackTime(AudioHandle handle);

	/**
	 * @brief Returns normalised playback position in [0, 1].
	 *
	 * Equivalent to @c getPlaybackTime() / @c getDuration(), clamped to [0,1].
	 * Returns 0 for inactive handles or zero-duration clips.
	 */
	[[nodiscard]] float getNormalizedTime(AudioHandle handle);

	/**
	 * @brief Returns seconds remaining until the end of the current loop.
	 *
	 * Equivalent to @c getDuration() - @c getPlaybackTime().
	 * Returns 0 for inactive handles.
	 */
	[[nodiscard]] float getRemainingTime(AudioHandle handle);

	/**
	 * @brief Returns the current gain of the voice.
	 *
	 * Reflects post-category-volume, post-fade gain. Returns 0 for inactive
	 * handles.
	 */
	[[nodiscard]] float getVolume(AudioHandle handle);

	/** @brief Returns the current pitch multiplier. Returns 1 for inactive handles. */
	[[nodiscard]] float getPitch(AudioHandle handle);

private:
	/** @brief Returns the snapshot for @p handle from the current read buffer. */
	[[nodiscard]]
	const VoiceView& getView(AudioHandle handle);

	AudioThread audioThread;
	Math::Random rng;
	AudioConfig config;
	std::atomic<U64> nextAudioHandle {1};
	bool initialized = false;
};

} // namespace Blackthorn::Audio
