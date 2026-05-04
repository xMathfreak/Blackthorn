#include "Saves/SaveId.h"

#include <chrono>

#include <sodium.h>

namespace Blackthorn::Saves {

SaveId SaveId::generate() {
	SaveId id;

	// Generate 16 random bytes via libsodium
	randombytes_buf(id.id.bytes.data(), 16);

	// Set version 4 bits: bits 12-15 of byte 6 = 0100
	id.id.bytes[6] = (id.id.bytes[6] & 0x0F) | 0x40;

	// Set variant bits: bits 6-7 of byte 8 = 10
	id.id.bytes[8] = (id.id.bytes[8] & 0x3F) | 0x80;

	const U64 now = static_cast<U64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count()
	);

	id.createdAt = now;
	id.updatedAt = now;

	return id;
}

} // namespace Blackthorn::Saves