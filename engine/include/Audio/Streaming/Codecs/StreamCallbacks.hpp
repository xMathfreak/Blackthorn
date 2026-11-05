#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Assets/AssetStream.h"
#include "Types/Numeric.h"

namespace Blackthorn::Audio::Streaming::StreamCallbacks {

enum class SeekOrigin {
	Begin,
	Current,
	End
};

/**
 * @brief Read callback for dr_wav, dr_flac, and dr_mp3. Forwards to the stream.
 *
 * @param user  The Assets::AssetStream* passed at init.
 * @param dst   Destination buffer from the decoder.
 * @param bytes Maximum bytes to read.
 * @return Bytes actually read. Fewer than requested means end of stream or error.
 */
inline size_t read(void* user, void* dst, size_t bytes) {
	auto* stream = static_cast<Assets::AssetStream*>(user);
	return stream->read(std::span<U8>(static_cast<U8*>(dst), bytes));
}

/**
 * @brief Seek callback shared by the dr_* libraries.
 *
 * The dr_* libraries only use "start" and "current" origins, so this takes a
 * bool instead of each library's origin enum. Each codec converts its own enum.
 *
 * @param user      The Assets::AssetStream* passed at init.
 * @param offset    Signed offset from the origin.
 * @param fromStart true for an origin at the start of the stream, false for the current position.
 * @return true on success.
 */
inline bool seek(void* user, int offset, SeekOrigin origin) {
	auto* stream = static_cast<Assets::AssetStream*>(user);

	U64 base = 0;

	switch (origin) {
		case SeekOrigin::Begin:
			base = 0;
			break;
		case SeekOrigin::Current:
			base = static_cast<I64>(stream->tell());
			break;
		case SeekOrigin::End:
			base = static_cast<I64>(stream->size());
			break;
	}

	if (offset < 0) {
		// Widen before negating to avoid overflow for INT_MIN
		const U64 distance = static_cast<U64>(
			-static_cast<I64>(offset)
		);

		if (distance > base)
			return false;

		return stream->seek(base - distance);
	}

	const U64 distance = static_cast<U64>(offset);

	if (distance > stream->size() - base)
		return false;

	return stream->seek(base + distance);
}

} // namespace Blackthorn::Audio::Streaming::StreamCallbacks
