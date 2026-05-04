#include "Saves/SaveId.h"

#include <chrono>
#include <iomanip>
#include <sstream>

#include <sodium.h>

namespace Blackthorn::Saves {

bool UUID::isNull() const noexcept {
	for (U8 b : bytes)
		if (b != 0) return false;

	return true;
}

std::string UUID::toString() const {
	std::ostringstream ss;
	ss << std::hex << std::setfill('0');

	for (int i = 0; i < 16; ++i) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			ss << '-';

		ss << std::setw(2) << static_cast<unsigned>(bytes[i]);
	}

	return ss.str();
}

UUID UUID::fromString(std::string_view str) {
	UUID uuid;

	std::string hex;
	hex.reserve(32);

	for (char c : str) {
		if (c != '-')
			hex += c;
	}

	if (hex.size() != 32)
		return uuid;

	for (size_t i = 0; i < 16; ++i) {
		const char hi = hex[i * 2];
		const char lo = hex[i * 2 + 1];

		auto fromHex = [](char c) -> int {
			if (c >= '0' && c <= '9')
				return c - '0';

			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;

			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;

			return -1;
		};

		int hi_val = fromHex(hi);
		int lo_val = fromHex(lo);

		if (hi_val < 0 || lo_val < 0)
			return UUID{};

		uuid.bytes[i] = static_cast<U8>((hi_val << 4) | lo_val);
	}

	return uuid;
}

UUID UUID::makeStable(std::string_view seed) {
	constexpr U64 FNV_PRIME = 1099511628211ULL;;
	const U64 BASIS_HI = 14695981039346656037ULL;
	const U64 BASIS_LO = 2166136261ULL;

	U64 hi = BASIS_HI;
	U64 lo = BASIS_LO;

	for (unsigned char c : seed) {
		hi ^= static_cast<U64>(c);
		hi *= FNV_PRIME;
		lo ^= static_cast<U64>(c);
		lo *= FNV_PRIME;
	}

	UUID uuid;

	for (int i = 0; i < 8; ++i) {
		uuid.bytes[i] = static_cast<U8>(hi >> (i * 8));;
		uuid.bytes[i + 8] = static_cast<U8>(lo >> (i * 8));
	}

	uuid.bytes[6] = (uuid.bytes[6] & 0x0F) | 0x40;
	uuid.bytes[8] = (uuid.bytes[8] & 0x3F) | 0x80;

	return uuid;
}

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