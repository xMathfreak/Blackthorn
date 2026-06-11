#pragma once

#include <optional>

#include "Audio/AudioConfig.h"
#include "Audio/Core/AudioThread.h"
#include "Core/Export.h"
#include "Math/NumericRange.h"
#include "Math/Random.h"

namespace Blackthorn::Audio {

/**
 * @brief Playback mode hint passed to @c AudioManager::play().
 *
 * @c Auto selects the mode based on @c AudioConfig::streamingThreshold:
 * clips whose estimated uncompressed size exceeds the threshold are
 * streamed; smaller clips are fully decoded into a resident AL buffer.
 * @c PCM and @c Stream override that heuristic explicitly.
 */
enum class PlaybackMode : U8 {
	Auto,
	PCM,
	Stream,
	Resident = PCM
};

/**
 * @brief Optional spatial audio parameters for @c PlayOptions.
 */
struct BLACKTHORN_API SpatialOptions {
	glm::vec3 position { 0.0f };
	float minDistance = 1.0f;
	float maxDistance = 50.0f;
};

/**
 * @brief Parameters for a single @c AudioManager::play() call.
 */
struct BLACKTHORN_API PlayOptions {
	Math::FloatRange volume = 1.0f; ///< Randomized each call if a range is set.
	Math::FloatRange pitch = 1.0f;

	AudioCategory category = AudioCategory::SFX;
	int priority = 0;

	bool loop = false;
	PlaybackMode mode = PlaybackMode::Auto;

	/// When set, the voice is positioned in world space and subject to
	/// distance attenuation. When absent, the voice plays at unit gain
	/// relative to the listener (non-spatial).
	std::optional<SpatialOptions> spatial = std::nullopt;
};

/**
 * @brief High-level audio interface for the game layer.
 *
 * @c AudioManager owns the @c AudioThread and exposes a command-based API
 * for starting, stopping, and querying voices. All commands are forwarded
 * to the audio thread asynchronously via an SPSC queue. None of these
 * methods block.
 *
 * @section querying Querying voice state
 * Voice state (playback position, flags, gain) is read from a
 * triple-buffered @c VoiceViewPool. Call @c update() once per frame to
 * latch the latest published snapshot; all query calls within that frame
 * then read a consistent, stable buffer. Querying without calling
 * @c update() first returns data from the previous latch, which may be
 * one or more frames stale.
 *
 * @section handle_lifetime Handle lifetime
 * @c play() returns an @c AudioHandle that remains valid until the voice
 * finishes naturally, is stolen by a higher-priority voice, or is
 * explicitly stopped. A stale handle returns @c PlaybackState::Inactive
 * from all query methods. It never aliases a different active voice
 * because handle IDs are never reused.
 */
class BLACKTHORN_API AudioManager {
public:
	AudioManager();
	~AudioManager();

	bool init(const AudioConfig& cfg = AudioConfig{});
	void shutdown();

	/**
	 * @brief Starts playback of @p clip and returns a handle to the voice.
	 *
	 * The handle is valid from the moment this returns until the voice
	 * is released. @p clip must remain valid for the lifetime of the voice.
	 */
	AudioHandle play(AudioClip& clip, const PlayOptions& options = {});

	void pause(AudioHandle handle);
	void resume(AudioHandle handle);
	void stop(AudioHandle handle);
	void setVolume(AudioHandle handle, float volume);
	void setCategoryVolume(AudioCategory category, float volume);
	void setPitch(AudioHandle handle, float pitch);
	void stopAll();
	void setPosition(AudioHandle handle, const glm::vec3& position);
	void setListenerTransform(
		const glm::vec3& position,
		const glm::vec3& forward,
		const glm::vec3& up,
		const glm::vec3& velocity
	);

	/**
	 * @brief Latches the latest audio thread snapshot for this frame.
	 *
	 * Swaps the read buffer with the most recently published write buffer
	 * in the triple-buffered @c VoiceViewPool. Call this once per frame
	 * before any query method. Subsequent query calls within the same frame
	 * read a consistent, immutable buffer.
	 *
	 * Thread safety: safe to call from the game thread at any time.
	 */
	void update();

	/** @brief Returns true if the handle refers to an active (non-released) voice. */
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
	 * @brief Returns the clip's total duration in seconds. 0 if inactive.
	 */
	[[nodiscard]] float getDuration(AudioHandle handle);

	/**
	 * @brief Returns the current playback position within the clip, in seconds.
	 *
	 * For resident clips: sourced from @c AL_SEC_OFFSET (exact).
	 * For streaming clips: @c (consumedFrames + AL_SAMPLE_OFFSET) / sampleRate,
	 * accurate to the sample regardless of decode-ahead depth.
	 *
	 * Returns 0 for inactive handles.
	 */
	[[nodiscard]] float getPlaybackTime(AudioHandle handle);

	/**
	 * @brief Returns normalized playback position in [0, 1].
	 *
	 * Equivalent to @c getPlaybackTime() / @c getDuration(), clamped.
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

	/** @brief Returns the current gain (post category-volume). 0 if inactive. */
	[[nodiscard]] float getVolume(AudioHandle handle);

	/** @brief Returns the current pitch multiplier. 1.0 if inactive. */
	[[nodiscard]] float getPitch(AudioHandle handle);

private:
	/** @brief Returns the view for @p handle from the current read buffer. */
	[[nodiscard]]
	const VoiceView& getView(AudioHandle handle);

	AudioThread audioThread;
	Math::Random rng;
	AudioConfig config;
	std::atomic<U64> nextAudioHandle { 1 };
	bool initialized = false;
};

} // namespace Blackthorn::Audio