#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

/**
 * @brief Named mixer bus categories.
 *
 * Every active voice belongs to exactly one category. Category volume is
 * multiplied with Master volume to produce the final gain applied to each
 * voice. Categories are a fixed enumeration to avoid string-keyed lookup on
 * the hot path; the set is intentionally small and covers most game audio
 * needs without turning into middleware-level complexity.
 *
 * The numeric values are stable and used as array indices inside
 * AudioManager, so never renumber existing entries.
 */
enum class AudioCategory : U8 {
	Master = 0, ///< Global multiplier applied on top of every other category.
	SFX = 1, ///< Short one-shot sound effects (footsteps, impacts, UI).
	Music = 2, ///< Background music tracks, typically looping and streamed.
	UI = 3, ///< Interface sounds (button clicks, notifications).
	Voice = 4, ///< Dialogue and narration.
	Ambient = 5, ///< Continuous environmental loops (wind, rain, crowd).

	Count ///< Sentinel. Do not use as a category.
};

inline constexpr size_t AUDIO_CATEGORY_COUNT =
	static_cast<size_t>(AudioCategory::Count);

/** @brief Returns a human-readable name for @p category. */
inline const char* audioCategoryName(AudioCategory category) noexcept {
	switch (category) {
		case AudioCategory::Master:
			return "Master";
		case AudioCategory::SFX:
			return "SFX";
		case AudioCategory::Music:
			return "Music";
		case AudioCategory::UI:
			return "UI";
		case AudioCategory::Voice:
			return "Voice";
		case AudioCategory::Ambient:
			return "Ambient";
		default:
			return "Unknown";
	}
}

} // namespace Blackthorn::Audio