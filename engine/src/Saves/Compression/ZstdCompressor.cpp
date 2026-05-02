#include "Saves/Compression/ZstdCompressor.h"

#include <algorithm>

#include <zstd.h>

#include "Debug/Logger.h"

namespace Blackthorn::Saves {

ZstdCompressor::ZstdCompressor(int level)
	: compressionLevel(std::clamp(level, MIN_LEVEL, MAX_LEVEL))
{}

bool ZstdCompressor::compress(
	const IO::ByteBuffer& input,
	IO::ByteBuffer& output
) {
	if (input.size() == 0) {
		output.clear();
		return true;
	}

	const size_t bound = ZSTD_compressBound(input.size());
	std::vector<U8> buf(bound);

	const size_t result = ZSTD_compress(
		buf.data(), bound,
		input.data(), input.size(),
		compressionLevel
	);

	if (ZSTD_isError(result)) {
		BT_ERROR("ZstdCompressor: compression failed: {}", ZSTD_getErrorName(result));
		return false;
	}

	output.clear();
	output.writeBytes(buf.data(), result);
	return true;
}

bool ZstdCompressor::decompress(
	const IO::ByteBuffer& input,
	IO::ByteBuffer& output,
	size_t expectedSize
) {
	if (input.size() == 0) {
		output.clear();
		return true;
	}

	size_t decompressedSize = ZSTD_getFrameContentSize(input.data(), input.size());

	if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
		decompressedSize = expectedSize > 0 ? expectedSize : input.size() * 4;
	} else if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
		BT_ERROR("ZstdCompressor: invalid zstd frame");
		return false;
	}

	std::vector<U8> buf(decompressedSize);

	const size_t result = ZSTD_decompress(
		buf.data(), decompressedSize,
		input.data(), input.size()
	);

	if (ZSTD_isError(result)) {
		BT_ERROR("ZstdCompressor: decompression failed: {}", ZSTD_getErrorName(result));
		return false;
	}

	output.clear();
	output.writeBytes(buf.data(), result);
	return true;
}

} // namespace Blackthorn::Saves