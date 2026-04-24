#include "Net/Protocol/FragmentAssembler.h"

#include "Debug/Logger.h"

namespace Blackthorn::Net::Protocol {

std::optional<Core::ByteBuffer> FragmentAssembler::ingest(
	const FragmentHeader& hdr,
	const Core::ByteBuffer& payload
) {
	if (hdr.totalFrags == 0 || hdr.fragIndex >= hdr.totalFrags) {
		BT_WARN(
			"FragmentAssembler: Malformed fragment header "
			"(id={}, index={}, total={}) — dropped",
			hdr.fragmentId, hdr.fragIndex, hdr.totalFrags
		);

		return std::nullopt;
	}

	const size_t incomingBytes = payload.size();
	FragmentSet* set = findSet(hdr.fragmentId);

	if (!set) {
		set = &allocateSet(hdr, incomingBytes);
	} else {

		if (set->totalFrags != hdr.totalFrags) {
			BT_WARN(
				"FragmentAssembler: Fragment count mismatch for id={} "
				"(expected {}, got {}) — evicting set",
				hdr.fragmentId, set->totalFrags, hdr.totalFrags
			);

			releaseSet(*set);
			set = &allocateSet(hdr, incomingBytes);
		}
	}

	if (set->fragments[hdr.fragIndex].size() > 0)
		return std::nullopt;

	set->fragments[hdr.fragIndex] = Core::ByteBuffer(
		payload.data() + payload.readPosition(),
		payload.remaining()
	);

	set->receivedCount++;
	set->totalBytesStored += incomingBytes;
	localBytes += incomingBytes;
	globalBytes += incomingBytes;

	if (!set->isComplete())
		return std::nullopt;

	Core::ByteBuffer result;
	result.reserve(set->totalBytesStored);

	for (Uint8 i = 0; i < set->totalFrags; ++i)
		result.writeBytes(set->fragments[i].data(), set->fragments[i].size());

	releaseSet(*set);
	return result;
}

void FragmentAssembler::evictExpired() {
	for (auto& slot : pool) {
		if (!slot.has_value())
			continue;

		if (slot->isExpired(EXPIRY_MS)) {
			BT_DEBUG(
				"FragmentAssembler: Evicting expired set id={}",
				slot->fragmentId
			);

			releaseSet(*slot);
		}
	}
}

void FragmentAssembler::reset() {
	for (auto& slot : pool) {
		if (slot.has_value()) {
			globalBytes -= slot->totalBytesStored;
			slot.reset();
		}
	}

	localBytes = 0;
}

FragmentSet* FragmentAssembler::findSet(Uint16 fragmentId) {
	for (auto& slot : pool) {
		if (slot.has_value() && slot->fragmentId == fragmentId)
			return &slot.value();
	}

	return nullptr;
}

FragmentSet& FragmentAssembler::allocateSet(
	const FragmentHeader& hdr,
	size_t incomingBytes
) {
	while (localBytes + incomingBytes > MAX_BYTES_PER_PEER) {
		BT_DEBUG("FragmentAssembler: Per-peer cap exceeded, evicting oldest");
		evictOldest();
	}

	constexpr size_t GLOBAL_CAP = 16u * 1024u * 1024u; // 16 MB
	while (globalBytes + incomingBytes > GLOBAL_CAP) {
		BT_DEBUG("FragmentAssembler: Global cap exceeded, evicting oldest");
		evictOldest();
	}

	for (auto& slot : pool) {
		if (!slot.has_value()) {
			slot.emplace();
			slot->fragmentId = hdr.fragmentId;
			slot->totalFrags = hdr.totalFrags;
			slot->receivedCount = 0;
			slot->firstFragmentMs = SDL_GetTicks();
			slot->fragments.assign(hdr.totalFrags, Core::ByteBuffer{});
			slot->totalBytesStored = 0;
			return slot.value();
		}
	}

	evictOldest();
	return allocateSet(hdr, incomingBytes);
}

void FragmentAssembler::releaseSet(FragmentSet& set) {
	const size_t bytes = set.totalBytesStored;
	localBytes -= (bytes <= localBytes) ? bytes : localBytes;
	globalBytes -= (bytes <= globalBytes) ? bytes : globalBytes;

	for (auto& slot : pool) {
		if (slot.has_value() && slot->fragmentId == set.fragmentId) {
			slot.reset();
			return;
		}
	}
}

void FragmentAssembler::evictOldest() {
	std::optional<FragmentSet>* oldest = nullptr;
	Uint64 oldestTime = std::numeric_limits<Uint64>::max();

	for (auto& slot : pool) {
		if (slot.has_value() && slot->firstFragmentMs < oldestTime) {
			oldestTime = slot->firstFragmentMs;
			oldest = &slot;
		}
	}

	if (oldest && oldest->has_value()) {
		BT_DEBUG(
			"FragmentAssembler: Evicting oldest set id={}",
			(*oldest)->fragmentId
		);

		releaseSet(oldest->value());
	}
}

} // namespace Blackthorn::Net::Transport::Channels