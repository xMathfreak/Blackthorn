#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

/**
 * @brief Lightweight handle to an active voice.
 *
 * @c AudioHandle is a value type that wraps a single monotonically
 * increasing @c U64 ID assigned by @c AudioManager when a play request
 * is made. IDs are never reused, so a stale handle from a finished or
 * stolen voice will never match any active voice in @c VoicePool::find().
 *
 * @section validity Validity
 * Zero is reserved as the invalid sentinel. All IDs assigned by
 * @c AudioManager start at 1 and increment strictly.
 *
 * @section thread_safety Thread safety
 * @c AudioHandle is a trivial value type. Copies and comparisons are safe
 * from any thread without synchronization.
 */
struct AudioHandle {
	/// Unique ID. Zero means invalid; all other values are permanently
	/// assigned and never reused across the lifetime of the process.
	U64 id = 0;

	/** @brief Returns true if this handle refers to a real voice request. */
	[[nodiscard]]
	bool isValid() const noexcept {
		return id != 0;
	}

	bool operator==(const AudioHandle&) const = default;
	bool operator!=(const AudioHandle&) const = default;

	/** @brief The canonical invalid handle. */
	static constexpr AudioHandle invalid() noexcept {
		return { 0 };
	}
};

} // namespace Blackthorn::Audio