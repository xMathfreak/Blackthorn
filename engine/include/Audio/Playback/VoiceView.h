#pragma once

#include "Audio/AudioHandle.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

/**
 * @brief Mutually exclusive playback state of a voice.
 */
enum class PlaybackState : U8 {
	/// No voice is active for this handle. The handle is stale or the
	/// voice finished and was released.
	Inactive,

	/// The voice is actively producing audio.
	Playing,

	/// The voice is paused. Resumable via @c AudioManager::resume().
	Paused,

	/// The voice has finished playing and is pending release, or was
	/// explicitly stopped. Distinguished from @c Inactive so callers can
	/// detect natural completion vs. never-started.
	Stopped,
};

/**
 * @brief Bit positions for @c VoiceFlags.
 *
 * Use @c VoiceFlags::test() to query individual bits rather than masking
 * directly.
 */
enum class VoiceFlagBit : U8 {
	Looping = 0, ///< Voice is set to loop continuously.
	Spatial = 1, ///< Voice has a world-space position.
	Streaming = 2, ///< Clip is streamed from disk (not fully resident).
	Fading = 3, ///< A volume fade is currently in progress.
};

/**
 * @brief Packed boolean properties of a voice.
 *
 * A single @c U8 holding up to 8 independent boolean attributes.
 * Multiple flags can be true simultaneously (e.g. Looping + Spatial).
 *
 * @code
 * VoiceFlags flags;
 * flags.set(VoiceFlagBit::Looping);
 *
 * if (flags.test(VoiceFlagBit::Streaming)) { ... }
 * @endcode
 */
struct VoiceFlags {
	U8 mask = 0;

	/** @brief Sets the bit corresponding to @p bit. */
	constexpr void set(VoiceFlagBit bit) noexcept {
		mask |= static_cast<U8>(1u << static_cast<U8>(bit));
	}

	/** @brief Clears the bit corresponding to @p bit. */
	constexpr void clear(VoiceFlagBit bit) noexcept {
		mask &= static_cast<U8>(~(1u << static_cast<U8>(bit)));
	}

	/** @brief Returns true if the bit corresponding to @p bit is set. */
	[[nodiscard]]
	constexpr bool test(VoiceFlagBit bit) const noexcept {
		return (mask >> static_cast<U8>(bit)) & 1u;
	}

	/** @brief Returns true if no flags are set. */
	[[nodiscard]]
	constexpr bool none() const noexcept {
		return mask == 0;
	}

	/** @brief Clears all flags. */
	constexpr void reset() noexcept {
		mask = 0;
	}
};

/**
 * @brief Point-in-time snapshot of a voice's observable state.
 * Published by the audio thread each tick via @c VoiceViewPool and
 * consumed by the game thread after @c AudioManager::update().
 */
struct VoiceView {
	/// Handle of the voice this snapshot belongs to.
	AudioHandle handle = AudioHandle::invalid();

	/// Mutually exclusive playback state.
	PlaybackState state = PlaybackState::Inactive;

	/// Non-exclusive boolean properties.
	VoiceFlags flags;

	/**
	 * @brief Current playback position within the clip, in seconds.
	 *
	 * Range is [0, duration]. For resident (non-streaming) clips this is
	 * sourced from @c AL_SEC_OFFSET, which reflects the AL mixer's exact
	 * consumption position. For streaming clips this is
	 * @c (consumedFrames + AL_SAMPLE_OFFSET) / sampleRate, where
	 * @c consumedFrames counts frames in fully-consumed AL buffers and
	 * @c AL_SAMPLE_OFFSET gives the mixer's position within the current
	 * buffer, accurate to the sample regardless of decode-ahead depth.
	 */
	float playbackPosition = 0.0f;

	/// Clip duration in seconds. Zero if the voice is inactive.
	float duration = 0.0f;

	/// Current gain (post category-volume and master-volume multiplication).
	float volume = 0.0f;

	/// Current pitch multiplier.
	float pitch = 1.0f;
};

} // namespace Blackthorn::Audio