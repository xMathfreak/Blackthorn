#pragma once

#include <vector>

#include "Audio/Playback/Voice.h"

namespace Blackthorn::Audio {

/**
 * @brief Fixed-capacity pool of @c Voice slots with priority-based stealing.
 *
 * @c VoicePool manages a flat array of @c Voice objects. Stale handle
 * detection relies on the monotonic, never-reused ID scheme of
 * @c AudioHandle: since IDs are never reused, a handle pointing to a
 * finished or stolen voice will simply never match any active voice in
 * @c find().
 *
 * @section acquire Acquiring a slot
 * @c acquire() returns a free slot if one exists, otherwise steals the
 * active voice with the lowest priority strictly less than
 * @p incomingPriority. Equal-priority incumbents are never stolen.
 * When two steal candidates share the lowest priority, the one with the
 * smaller @c startTick() is evicted first (FIFO).
 *
 * @section internals Internal structure
 * Two parallel structures are maintained alongside @c voiceStorage:
 *
 * - @c handleIds: a @c vector<U64> mirroring @c voice[i].handle().id
 *   at index @c i. Elements are 8 bytes each, so the entire array fits
 *   in two cache lines at the default pool size of 32. @c find() scans
 *   this array instead of the @c Voice objects, avoiding touching any
 *   @c Voice fields until a match is confirmed.
 *
 * - @c freeSlots: a stack of free slot indices. @c acquire() pops in
 *   O(1); @c release() pushes in O(1). @c findStealCandidate() still
 *   scans @c voiceStorage because it must compare priority and tick
 *   fields but it only runs when the pool is completely full.
 */
class VoicePool {
public:
	explicit VoicePool(size_t maxVoices = 32);

	/**
	 * @brief Acquires a free or stealable slot.
	 *
	 * @param priority Steal priority of the incoming request.
	 * @return         Pointer to the acquired @c Voice, or @c nullptr if
	 *                 the pool is full and no voice can be stolen.
	 */
	Voice* acquire(int priority);

	/**
	 * @brief Looks up an active voice by handle.
	 *
	 * Matches @c voice.handle().id == handle.id. Since IDs are never
	 * reused, a stale handle will never match an active voice. Returns
	 * @c nullptr for invalid or unmatched handles.
	 */
	Voice* find(AudioHandle handle);

	/**
	 * @brief Releases a slot, calling @c Voice::reset().
	 *
	 * After this call the slot is free. Any handle pointing to it becomes
	 * stale and will fail future @c find() calls.
	 */
	void release(Voice& voice);

	/** @brief Stops and releases all active voices. */
	void stopAll();

	/**
	 * @brief Reclaims naturally finished voices.
	 *
	 * Called once per audio thread tick.
	 * - Non-streaming: released when @c AL_STOPPED.
	 * - Streaming: released when @c endOfStream is set AND @c AL_STOPPED.
	 */
	void update();

	[[nodiscard]] size_t size() const noexcept;

	/**
	 * @brief Read-only view of all slots (active and inactive).
	 *
	 * Callers must check @c Voice::active() before accessing voice state.
	 */
	[[nodiscard]]
	const std::vector<Voice>& voices() const noexcept;

	[[nodiscard]]
	std::vector<Voice>& voices() noexcept;

private:
	Voice* findStealCandidate(int incomingPriority);

private:
	std::vector<Voice> voiceStorage;

	/**
	 * @brief Parallel array of active handle IDs, indexed by slot.
	 *
	 * @c handleIds[i] == voiceStorage[i].handle().id when slot @c i
	 * is active, and 0 when it is free. Kept synchronized by @c acquire(),
	 * @c release() and @c stopAll().
	 */
	std::vector<U64> handleIds;

	/**
	 * @brief Stack of free slot indices.
	 *
	 * Populated with @c {0...n-1} at construction. @c acquire() pops
	 * the back; @c release() pushes the freed index. Never exceeds @c
	 * maxVoices entries.
	 */
	std::vector<size_t> freeSlots;
};

} // namespace Blackthorn::Audio