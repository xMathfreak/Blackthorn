#pragma once

#include <array>
#include <optional>
#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "IO/ByteBuffer.h"
#include "Net/Protocol/FragmentHeader.h"

namespace Blackthorn::Net::Protocol {

/**
 * @brief Holds the in-flight state for a single incomplete fragmented message.
 */
struct BLACKTHORN_API FragmentSet {
	/// Unique ID for this message, matching @c FragmentHeader::fragmentId.
	U16 fragmentId = 0;

	/// Expected total number of fragments.
	U8 totalFrags = 0;

	/// Number of fragments received so far.
	U8 receivedCount = 0;

	/// SDL_GetTicks() when the first fragment expired. Used for expiry
	U64 firstFragmentMs = 0;

	/**
	 * @brief Per fragment payloads.
	 *
	 * Index corresponds to @c FragmentHeader::fragIndex.
	 * Empty slots have zero size until the fragment arrives.
	 */
	std::vector<IO::ByteBuffer> fragments;

	/// Total bytes stored across all received fragments.
	size_t totalBytesStored = 0;

	bool isComplete() const noexcept {
		return receivedCount == totalFrags;
	}

	bool isExpired(U32 timeoutMs) const noexcept {
		return (SDL_GetTicks() - firstFragmentMs) > timeoutMs;
	}
};

class BLACKTHORN_API FragmentAssembler {
public:
	/// Maximum simultaneous incomplete fragment sets per peer.
	static constexpr size_t MAX_SETS = 16;

	/// In-flight reassembly budget per peer in bytes.
	static constexpr size_t MAX_BYTES_PER_PEER = 512u * 1024u; // 512 KB

	/// Milliseconds before an incomplete set is considered stale.
	static constexpr U32 EXPIRY_MS = 1500u;

	/**
	 * @brief Constructs an assembler that contributes to a shared
	 * global byte counter.
	 *
	 * @param globalBytesUsed Reference to the engine-wide reassembly
	 * 		  budget counter.
	 */
	explicit FragmentAssembler(size_t& globalBytesUsed)
		: globalBytes(globalBytesUsed)
	{}

	FragmentAssembler(const FragmentAssembler&) = delete;
	FragmentAssembler& operator=(const FragmentAssembler&) = delete;

	FragmentAssembler(FragmentAssembler&&) = default;
	FragmentAssembler& operator=(FragmentAssembler&&) = delete;

	/**
	 * @brief Ingests one fragment and returns the reassembled message
	 * if all fragments have now arrived.
	 *
	 * @param header Deserialized @c FragmentHeader for this datagram.
	 * @param payload Raw bytes of this fragment's payload slice.
	 * 				  For fragment 0 this includes @c PacketHeader.
	 * 				  For subsequent fragments it is pure payload.
	 *
	 * @return The complete reassembled buffer (PacketHeader + full payload)
	 * 		   if the set is now complete, or @c std::nullopt if more
	 * 		   fragments are still needed.
	 */
	std::optional<IO::ByteBuffer> ingest(
		const FragmentHeader& header,
		const IO::ByteBuffer& payload
	);

	/**
	 * @brief Evicts all fragment sets that have exceeded @c EXPIRY_MS.
	 *
	 * Call once per I/O poll iteration from @c NetworkIOWorker::ioThreadLoop().
	 */
	void evictExpired();

	/**
	 * @brief discards all in-flight sets are resets byte counters.
	 */
	void reset();

	/**
	 * @brief Bytes currently buffered in this assembler.
	 */
	size_t bytesBuffered() const noexcept { return localBytes; }

private:
	/// Finds the FragmentSet for @p fragmentId or nullptr;
	FragmentSet* findSet(U16 fragmentId);

	/// Allocates a new FragmentSet, evicting oldest if the pool
	/// is full or memory caps would be exceeded.
	FragmentSet& allocateSet(const FragmentHeader& hdr, size_t incomingBytes);

	/// Releases a set and updates both byte counters.
	void releaseSet(FragmentSet& set);

	/// Evicts the oldest set in the pool.
	void evictOldest();

	std::array<std::optional<FragmentSet>, MAX_SETS> pool;

	size_t localBytes = 0; ///< Bytes buffered in this peer's assembler.
	size_t& globalBytes; ///< Shared engine-wide counter.
};

} //namespace Blackthorn::Net::Protocol