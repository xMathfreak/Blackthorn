#pragma once

#include "Core/Export.h"
#include "IO/ByteBuffer.h"

namespace Blackthorn::Saves {

/**
 * @brief Interface for a symmetric compression pass in the save pipeline.
 *
 * The save pipeline applies compression before encryption:
 *
 *   plaintext  →  compress()  →  encrypt()  →  disk
 *   disk       →  decrypt()   →  decompress() →  plaintext
 *
 * Both directions operate on raw byte spans to keep the interface
 * independent of the buffer type used by the calling layer.
 */
class BLACKTHORN_API ICompressor {
public:
	virtual ~ICompressor() = default;

	/**
	 * @brief Compresses @p input into @p output.
	 * @param input  Source bytes.
	 * @param output Destination buffer. Existing contents are cleared.
	 * @return true on success.
	 */
	virtual bool compress(
		const IO::ByteBuffer& input,
		IO::ByteBuffer& output
	) = 0;

	/**
	 * @brief Decompresses @p input into @p output.
	 * @param input          Compressed source bytes.
	 * @param output         Destination buffer. Existing contents are cleared.
	 * @param expectedSize   Expected decompressed size in bytes, used to
	 *                       pre-allocate @p output. Pass 0 if unknown.
	 * @return true on success.
	 */
	virtual bool decompress(
		const IO::ByteBuffer& input,
		IO::ByteBuffer& output,
		size_t expectedSize = 0
	) = 0;
};

} // namespace Blackthorn::Saves#pragma once

namespace Blackthorn::Saves {

} // namespace Blackthorn::Saves