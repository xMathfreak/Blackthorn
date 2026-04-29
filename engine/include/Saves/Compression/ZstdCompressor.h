#pragma once

#include "Core/Export.h"
#include "Saves/Compression/ICompressor.h"

namespace Blackthorn::Saves {

/**
 * @brief zstd compressor implementation.
 *
 * Uses the zstd streaming API so there is no hard limit on the size of data
 * that can be processed in a single call.
 *
 * @par Compression levels
 * zstd compression levels range from 1 (fastest, lowest ratio) to 22
 * (slowest, highest ratio). For save files the recommended range is 1–6:
 *
 * - Level 1: Suitable for autosaves where write latency matters.
 * - Level 3: Good default for manual saves (default).
 * - Level 6: Acceptable for one-time exports where size matters more than speed.
 *
 * Levels above 6 offer diminishing returns for typical game save data and
 * are not recommended at runtime.
 */
class BLACKTHORN_API ZstdCompressor final : public ICompressor {
public:
	static constexpr int DEFAULT_LEVEL = 3;
	static constexpr int MIN_LEVEL = 1;
	static constexpr int MAX_LEVEL = 22;

	/**
	 * @brief Constructs a compressor with the specified compression level.
	 * @param level zstd compression level. Clamped to [MIN_LEVEL, MAX_LEVEL].
	 */
	explicit ZstdCompressor(int level = DEFAULT_LEVEL);

	bool compress(
		const IO::ByteBuffer& input,
		IO::ByteBuffer& output
	) override;

	bool decompress(
		const IO::ByteBuffer& input,
		IO::ByteBuffer& output,
		size_t expectedSize = 0
	) override;

	int getLevel() const noexcept { return compressionLevel; }

private:
	int compressionLevel;
};

} // namespace Blackthorn::Saves