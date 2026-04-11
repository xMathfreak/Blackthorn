#pragma once

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"

namespace Blackthorn::Net {

/**
 * @brief Flat read/write byte buffer with explicit little-endian encoding.
 *
 * All multi-byte values are written and read in little-endian order,
 * regardless of host platform. This ensures the wire format is identical
 * on every machine that participates in a session.
 *
 * Write mode and read mode share the same underlying storage. Typical
 * usage is to write into the buffer, then either pass the raw span to a
 * transport layer or wrap the buffer in a new ByteBuffer constructed from
 * the written bytes for reading.
 *
 * @code
 * // Writing
 * ByteBuffer out;
 * out.writeU32(42);
 * out.writeF32(3.14f);
 * out.writeString("hello");
 *
 * // Reading
 * ByteBuffer in(out.data(), out.size());
 * Uint32 n   = in.readU32();
 * float  f   = in.readF32();
 * auto   str = in.readString();
 * @endcode
 *
 * @note This class is not thread-safe. External synchronisation is
 * required if the buffer is shared across threads.
 */
class BLACKTHORN_API ByteBuffer {
public:
	/** @brief Constructs an empty writable buffer. */
	ByteBuffer() = default;

	/**
	 * @brief Constructs a read buffer from an existing byte span.
	 *
	 * Copies the provided bytes into internal storage and positions the
	 * read cursor at the beginning. The original pointer does not need to
	 * remain valid after construction.
	 *
	 * @param data Pointer to source bytes.
	 * @param size Number of bytes to copy.
	 */
	ByteBuffer(const Uint8* data, size_t size)
		: buffer(data, data + size)
		, readCursor(0)
	{}

	/**
	 * @brief Constructs a read buffer from an existing vector, taking
	 * ownership of its contents via move.
	 */
	explicit ByteBuffer(std::vector<Uint8>&& data)
		: buffer(std::move(data))
		, readCursor(0)
	{}

	void writeU8(Uint8 v) {
		buffer.push_back(v);
	}

	void writeI8(Sint8 v) {
		writeU8(static_cast<Uint8>(v));
	}

	void writeU16(Uint16 v) {
		buffer.push_back(static_cast<Uint8>(v));
		buffer.push_back(static_cast<Uint8>(v >> 8));
	}

	void writeI16(Sint16 v) {
		writeU16(static_cast<Uint16>(v));
	}

	void writeU32(Uint32 v) {
		buffer.push_back(static_cast<Uint8>(v));
		buffer.push_back(static_cast<Uint8>(v >> 8));
		buffer.push_back(static_cast<Uint8>(v >> 16));
		buffer.push_back(static_cast<Uint8>(v >> 24));
	}

	void writeI32(Sint32 v) {
		writeU32(static_cast<Uint32>(v));
	}

	void writeU64(Uint64 v) {
		writeU32(static_cast<Uint32>(v));
		writeU32(static_cast<Uint32>(v >> 32));
	}

	void writeI64(Sint64 v) {
		writeU64(static_cast<Uint64>(v));
	}

	void writeF32(float v) {
		Uint32 bits;
		std::memcpy(&bits, &v, sizeof(bits));
		writeU32(bits);
	}

	void writeF64(double v) {
		Uint64 bits;
		std::memcpy(&bits, &v, sizeof(bits));
		writeU64(bits);
	}

	void writeBool(bool v) {
		writeU8(v ? 1u : 0u);
	}

	/**
	 * @brief Writes a length-prefixed UTF-8 string.
	 *
	 * The length is stored as a Uint16, limiting strings to 65535 bytes.
	 *
	 * @param str String to write.
	 * @throws std::length_error if the string exceeds 65535 bytes.
	 */
	void writeString(const std::string& str) {
		if (str.size() > 65535)
			throw std::length_error("ByteBuffer::writeString: string exceeds 65535 bytes");

		writeU16(static_cast<Uint16>(str.size()));
		writeBytes(reinterpret_cast<const Uint8*>(str.data()), str.size());
	}

	/**
	 * @brief Writes raw bytes directly into the buffer.
	 * @param data Pointer to source bytes.
	 * @param size Number of bytes to write.
	 */
	void writeBytes(const Uint8* data, size_t size) {
		buffer.insert(buffer.end(), data, data + size);
	}

	Uint8 readU8() {
		checkAvailable(1);
		return buffer[readCursor++];
	}

	Sint8 readI8() {
		return static_cast<Sint8>(readU8());
	}

	Uint16 readU16() {
		checkAvailable(2);
		Uint16 v = static_cast<Uint16>(buffer[readCursor])
				 | static_cast<Uint16>(buffer[readCursor + 1]) << 8;
		readCursor += 2;
		return v;
	}

	Sint16 readI16() {
		return static_cast<Sint16>(readU16());
	}

	Uint32 readU32() {
		checkAvailable(4);
		Uint32 v = static_cast<Uint32>(buffer[readCursor])
				 | static_cast<Uint32>(buffer[readCursor + 1]) << 8
				 | static_cast<Uint32>(buffer[readCursor + 2]) << 16
				 | static_cast<Uint32>(buffer[readCursor + 3]) << 24;

		readCursor += 4;
		return v;
	}

	Sint32 readI32() {
		return static_cast<Sint32>(readU32());
	}

	Uint64 readU64() {
		Uint64 lo = readU32();
		Uint64 hi = readU32();
		return lo | (hi << 32);
	}

	Sint64 readI64() {
		return static_cast<Sint64>(readU64());
	}

	float readF32() {
		Uint32 bits = readU32();
		float v;
		std::memcpy(&v, &bits, sizeof(v));
		return v;
	}

	double readF64() {
		Uint64 bits = readU64();
		double v;
		std::memcpy(&v, &bits, sizeof(v));
		return v;
	}

	bool readBool() {
		return readU8() != 0;
	}

	/**
	 * @brief Reads a length-prefixed UTF-8 string written by writeString().
	 */
	std::string readString() {
		Uint16 len = readU16();
		checkAvailable(len);
		std::string str(reinterpret_cast<const char*>(&buffer[readCursor]), len);
		readCursor += len;
		return str;
	}

	/**
	 * @brief Reads exactly `size` bytes into `dest`.
	 * @param dest Destination buffer. Must be at least `size` bytes.
	 * @param size Number of bytes to read.
	 */
	void readBytes(Uint8* dest, size_t size) {
		checkAvailable(size);
		std::memcpy(dest, &buffer[readCursor], size);
		readCursor += size;
	}

	/**
	 * @brief Overwrites 4 bytes at `offset` with `v` in little-endian order.
	 *
	 * Used to back-patch length or size fields written before the data
	 * that determines their value.
	 *
	 * @param offset Byte offset of the field to overwrite.
	 * @param v      Value to write.
	 */
	void patchU32(size_t offset, Uint32 v) {
		assert(offset + 4 <= buffer.size());
		buffer[offset] = static_cast<Uint8>(v);
		buffer[offset + 1] = static_cast<Uint8>(v >> 8);
		buffer[offset + 2] = static_cast<Uint8>(v >> 16);
		buffer[offset + 3] = static_cast<Uint8>(v >> 24);
	}

	/** @brief Returns a pointer to the raw buffer contents. */
	const Uint8* data() const { return buffer.data(); }

	/** @brief Returns the total number of bytes written. */
	size_t size() const { return buffer.size(); }

	/** @brief Returns true if no bytes have been written. */
	bool empty() const { return buffer.empty(); }

	/** @brief Returns the current read cursor position. */
	size_t readPosition() const { return readCursor; }

	/** @brief Returns the number of bytes remaining to be read. */
	size_t remaining() const {
		return readCursor <= buffer.size() ? buffer.size() - readCursor : 0;
	}

	/** @brief Returns true if the read cursor has reached the end. */
	bool exhausted() const { return readCursor >= buffer.size(); }

	/**
	 * @brief Advances the read cursor by `count` bytes without returning data.
	 *
	 * Used by the snapshot system to move past component data for components
	 * whose fixed wire size is known without deserializing them fully.
	 *
	 * @param count Number of bytes to skip.
	 * @throws std::out_of_range if skipping would move past the end.
	 */
	void skip(size_t count) {
		checkAvailable(count);
		readCursor += count;
	}

	/**
	 * @brief Resets the read cursor to the beginning without clearing data.
	 *
	 * Allows re-reading the same buffer from the start.
	 */
	void resetRead() { readCursor = 0; }

	/** @brief Clears all written data and resets the read cursor. */
	void clear() {
		buffer.clear();
		readCursor = 0;
	}

	/**
	 * @brief Reserves capacity in the underlying storage.
	 *
	 * Avoids repeated reallocations when the final size is approximately
	 * known in advance.
	 *
	 * @param capacity Number of bytes to reserve.
	 */
	void reserve(size_t capacity) {
		buffer.reserve(capacity);
	}

private:
	std::vector<Uint8> buffer;
	size_t readCursor = 0;

	void checkAvailable(size_t needed) const {
		if (readCursor + needed > buffer.size()) {
			throw std::out_of_range(
				"ByteBuffer: read past end of buffer ("
				+ std::to_string(readCursor + needed)
				+ " > "
				+ std::to_string(buffer.size())
				+ ")"
			);
		}
	}
};

} // namespace Blackthorn::Net