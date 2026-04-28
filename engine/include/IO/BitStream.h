#pragma once

#include <cassert>

#include "Core/Export.h"
#include "Core/Types/Types.h"
#include "IO/ByteBuffer.h"

namespace Blackthorn::IO {

namespace BitCodec {

constexpr U32 zigzagEncode(I32 v) {
	return static_cast<U32>((v << 1) ^ (v >> 31));
}

constexpr I32 zigzagDecode(U32 v) {
	return static_cast<I32>((v >> 1) ^ (-(v & 1u)));
}

} // namespace BitCodec

/**
 * @brief Writes individual bits and small integers into a ByteBuffer.
 *
 * Bits are accumulated in a 32-bit word and flushed to the buffer in
 * complete bytes as they fill. Call `flush()` when all fields have been
 * written to emit any partial byte remaining in the accumulator.
 *
 * Signed integers are encoded using zigzag encoding so that small
 * negative values (e.g. -1, -2) produce small bit widths rather than
 * requiring all 32 or 64 bits.
 *
 * Zigzag encoding:  n >= 0  ->  2n       (0->0, 1->2, 2->4)
 *                   n <  0  ->  2|n| - 1 (-1->1, -2->3, -3->5)
 *
 * @code
 * ByteBuffer buf;
 * BitPacker packer(buf);
 * packer.writeBool(true);
 * packer.writeBits(7u, 3);      // value 7 in 3 bits
 * packer.writeSignedBits(-3, 4); // zigzag-encoded in 4 bits
 * packer.flush();
 * @endcode
 *
 * @note A `BitPacker` holds a mutable reference to its `ByteBuffer`.
 * The buffer must outlive the packer.
 */
class BLACKTHORN_API BitPacker {
public:
	/**
	 * @brief Constructs a BitPacker writing into `buffer`.
	 * @param buffer Destination buffer. Must outlive this packer.
	 */
	explicit BitPacker(ByteBuffer& buffer)
		: buf(buffer)
	{}

	~BitPacker() {
		// Flush any remaining bits on destruction so callers that forget
		// to call flush() don't silently lose data in debug builds.
		if (bitCount > 0)
			flush();
	}

	BitPacker(const BitPacker&) = delete;
	BitPacker& operator=(const BitPacker&) = delete;

	/**
	 * @brief Writes a single boolean as one bit.
	 * @param v Value to write.
	 */
	void writeBool(bool v) {
		writeBits(v ? 1u : 0u, 1);
	}

	/**
	 * @brief Writes the low `bits` of `value` into the stream.
	 *
	 * @param value Unsigned integer value to write.
	 * @param bits  Number of bits to write. Must be in [1, 32].
	 */
	void writeBits(U32 value, int bits) {
		assert(bits >= 1 && bits <= 32);

		// Mask off any stray high bits to keep the accumulator clean.
		if (bits < 32)
			value &= (1u << bits) - 1u;

		accumulator |= (value << bitCount);
		bitCount += bits;

		while (bitCount >= 8) {
			buf.writeU8(static_cast<U8>(accumulator & 0xFF));
			accumulator >>= 8;
			bitCount -= 8;
		}
	}

	/**
	 * @brief Writes a signed integer using zigzag encoding.
	 *
	 * Zigzag encoding maps signed integers to unsigned integers so that
	 * small-magnitude values (positive and negative) require fewer bits.
	 *
	 * @param value Signed integer to encode.
	 * @param bits  Number of bits for the zigzag-encoded result. Must be
	 *              in [1, 32]. The representable signed range is
	 *              [-(2^(bits-1)), 2^(bits-1) - 1].
	 */
	void writeSignedBits(I32 value, int bits) {
		assert(bits >= 1 && bits <= 32);
		U32 encoded = BitCodec::zigzagEncode(value);
		writeBits(encoded, bits);
	}

	/**
	 * @brief Flushes any remaining bits as a partial byte, zero-padded.
	 *
	 * Must be called after all fields have been written. The corresponding
	 * `BitReader` will automatically skip the padding bits if it reads
	 * the same fields in the same order.
	 */
	void flush() {
		if (bitCount > 0) {
			buf.writeU8(static_cast<U8>(accumulator & 0xFF));
			accumulator = 0;
			bitCount = 0;
		}
	}

	/** @brief Returns the number of bits currently held in the accumulator. */
	int pendingBits() const { return bitCount; }

private:
	ByteBuffer& buf;
	U32 accumulator = 0;
	int bitCount = 0;
};

/**
 * @brief Reads individual bits and small integers from a ByteBuffer.
 *
 * Mirrors `BitPacker` exactly. Fields must be read in the same order
 * and with the same bit widths they were written.
 *
 * @code
 * BitReader reader(buf);
 * bool flag    = reader.readBool();
 * U32 val   = reader.readBits(3);
 * I32 delta = reader.readSignedBits(4);
 * @endcode
 */
class BLACKTHORN_API BitReader {
public:
	/**
	 * @brief Constructs a BitReader reading from `buffer`.
	 * @param buffer Source buffer. Must outlive this reader.
	 */
	explicit BitReader(ByteBuffer& buffer)
		: buf(buffer)
	{}

	BitReader(const BitReader&) = delete;
	BitReader& operator=(const BitReader&) = delete;

	/**
	 * @brief Reads one bit as a boolean.
	 */
	bool readBool() {
		return readBits(1) != 0;
	}

	/**
	 * @brief Reads `bits` bits and returns them as an unsigned integer.
	 * @param bits Number of bits to read. Must be in [1, 32].
	 */
	[[nodiscard]]
	U32 readBits(int bits) {
		assert(bits >= 1 && bits <= 32);

		while (bitCount < bits) {
			U32 byte = buf.readU8();
			accumulator |= (byte << bitCount);
			bitCount += 8;
		}

		U32 mask = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
		U32 value = accumulator & mask;
		accumulator >>= bits;
		bitCount -= bits;
		return value;
	}

	/**
	 * @brief Reads `bits` bits and decodes them as a signed integer via
	 * zigzag decoding.
	 *
	 * @param bits Number of bits the value was encoded into.
	 */
	[[nodiscard]]
	I32 readSignedBits(int bits) {
		return BitCodec::zigzagDecode(readBits(bits));
	}

private:
	ByteBuffer& buf;
	U32 accumulator = 0;
	int bitCount = 0;
};

} // namespace Blackthorn::IO