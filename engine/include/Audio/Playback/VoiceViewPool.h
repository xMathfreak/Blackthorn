#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "Audio/Playback/VoiceView.h"

namespace Blackthorn::Audio {

/**
 * @brief Triple-buffered pool of per-voice state snapshots.
 */
class VoiceViewPool {
public:
	explicit VoiceViewPool(size_t capacity);

	[[nodiscard]]
	VoiceView& writeSlot(size_t slotIndex);

	void publish();

	void acquire();

	[[nodiscard]]
	const VoiceView& query(AudioHandle handle) const;

	/** @brief Returns the total number of voice slots per buffer. */
	[[nodiscard]]
	size_t capacity() const noexcept;

private:
	using ViewBuffer = std::vector<VoiceView>;

	std::array<ViewBuffer, 3> buffers;

	int writeIndex = 0;
	int readIndex = 1;

	std::atomic<int> spareIndex {2};

	/// Returned when query() finds no matching voice. Always Inactive.
	static constexpr VoiceView inactiveDefault{};
};

} // namespace Blackthorn::Audio