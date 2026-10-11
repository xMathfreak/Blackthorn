#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <system_error>

#include "Core/Export.h"
#include "FileIO.h"
#include "Types/Numeric.h"

#include "Debug/Logger.h"
namespace Blackthorn::Assets {

/**
 * @class AssetStream
 * @brief Incremental, seekable byte source for one asset.
 *
 * A stream is a view over one asset's uncompressed bytes. It holds its own file
 * handle, so it stays valid if the pack it came from is unmounted, and it must be
 * used by one thread at a time.
 *
 * read() returns 0 at end of stream and also on error. Check ok() after a short
 * or zero read to tell the two apart.
 */
class BLACKTHORN_API AssetStream {
public:
	virtual ~AssetStream() = default;

	/// Total uncompressed size of the asset, in bytes.
	virtual U64 size() const = 0;

	/// Current read position.
	virtual U64 tell() const = 0;

	/**
	 * @brief Moves the read position to @p offset, from the start of the asset.
	 * @return false if @p offset is past the end, or the seek failed.
	 */
	virtual bool seek(U64 offset) = 0;

	/**
	 * @brief Reads up to dst.size() bytes at the current position.
	 * @return The number of bytes read. 0 means end of stream or an error; check ok().
	 */
	virtual size_t read(std::span<U8> dst) = 0;

	/// False once an I/O error has occurred. Stays false for the rest of the stream's life.
	virtual bool ok() const = 0;
};

namespace Detail {

struct FileCloser {
	void operator()(std::FILE* f) const noexcept {
		if (f)
			std::fclose(f);
	}
};

using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

} // namespace Detail

/**
 * @brief Streams a loose file from disk.
 *
 * Use LooseAssetStream::open(); the constructor is private so a stream can't
 * exist without an open file.
 */
class BLACKTHORN_API LooseAssetStream final : public AssetStream {
public:
	/**
	 * @return The stream, or nullptr if the file can't be opened or sized.
	 */
	static std::unique_ptr<LooseAssetStream> open(const std::filesystem::path& path) {
		std::error_code ec;
		const auto fileSize = std::filesystem::file_size(path, ec);
		if (ec)
			return nullptr;

		Detail::FilePtr file(std::fopen(path.string().c_str(), "rb"));
		if (!file)
			return nullptr;

		return std::unique_ptr<LooseAssetStream>(
			new LooseAssetStream(std::move(file), static_cast<U64>(fileSize))
		);
	}

	U64 size() const override { return fileSize; }
	U64 tell() const override { return position; }

	bool seek(U64 offset) override {
		if (offset > fileSize || !Blackthorn::seekTo(file.get(), offset)) {
			failed = true;
			return false;
		}

		position = offset;
		return true;
	}

	size_t read(std::span<U8> dst) override {
		const U64 remaining = fileSize - position;
		const size_t want = static_cast<size_t>(std::min<U64>(dst.size(), remaining));
		if (want == 0)
			return 0;

		const size_t got = std::fread(dst.data(), 1, want, file.get());
		position += got;

		if (got != want && std::ferror(file.get()))
			failed = true;

		return got;
	}

	bool ok() const override { return !failed; }

private:
	LooseAssetStream(Detail::FilePtr f, U64 sz)
		: file(std::move(f))
		, fileSize(sz)
	{}

	Detail::FilePtr file;
	U64 fileSize = 0;
	U64 position = 0;
	bool failed = false;
};

/**
 * @brief Streams an uncompressed entry from a .btp pack file.
 *
 * Each stream opens its own handle on the pack and reads from dataOffset. Only
 * uncompressed entries can be streamed; compressed entries are rejected by
 * PackMount::open().
 */
class BLACKTHORN_API PackedAssetStream final : public AssetStream {
public:
	/**
	 * @brief Opens a stream over one entry's bytes.
	 *
	 * Checks that the pack file is large enough to contain the entry. This is the
	 * only integrity check on open, and it catches truncated packs. Bit errors are
	 * caught only by a full read through resolve(), which verifies the payload hash.
	 *
	 * @return The stream, or nullptr if the pack can't be opened or is truncated.
	 */
	static std::unique_ptr<PackedAssetStream> open(
		const std::filesystem::path& packPath,
		U64 dataOffset,
		U64 length
	) {
		std::error_code ec;
		const auto packSize = std::filesystem::file_size(packPath, ec);
		if (ec)
			return nullptr;

		const U64 packBytes = static_cast<U64>(packSize);
		if (dataOffset > packBytes || length > packBytes - dataOffset)
			return nullptr;

		Detail::FilePtr file(std::fopen(packPath.string().c_str(), "rb"));
		if (!file)
			return nullptr;

		return std::unique_ptr<PackedAssetStream>(
			new PackedAssetStream(std::move(file), dataOffset, length)
		);
	}

	U64 size() const override { return length; }
	U64 tell() const override { return position; }

	bool seek(U64 offset) override {
		if (offset > length || !Blackthorn::seekTo(file.get(), dataOffset + offset)) {
			failed = true;
			return false;
		}

		position = offset;
		return true;
	}

	size_t read(std::span<U8> dst) override {
		const U64 remaining = length - position;
		const size_t want = static_cast<size_t>(std::min<U64>(dst.size(), remaining));
		if (want == 0)
			return 0;

		const size_t got = std::fread(dst.data(), 1, want, file.get());
		position += got;

		if (got != want && std::ferror(file.get()))
			failed = true;

		return got;
	}

	bool ok() const override { return !failed; }

private:
	PackedAssetStream(Detail::FilePtr f, U64 offset, U64 len)
		: file(std::move(f))
		, dataOffset(offset)
		, length(len)
	{
		// Position the handle at the start of the entry. The open succeeded, so this seek is expected to succeed.
		Blackthorn::seekTo(file.get(), dataOffset);
	}

	Detail::FilePtr file;
	U64 dataOffset = 0;
	U64 length = 0;
	U64 position = 0;
	bool failed = false;
};

/**
 * @brief Streams from a byte range owned by someone else.
 *
 * Non-owning. The bytes must stay alive for as long as the stream is in use.
 * Used for clips small enough to keep in memory. The clip that owns the bytes
 * must outlive any voice streaming it, the same rule in-memory decoders already
 * followed.
 */
class BLACKTHORN_API MemoryAssetStream final : public AssetStream {
public:
	MemoryAssetStream(const U8* data, size_t byteCount)
		: bytes(data)
		, length(byteCount)
	{}

	U64 size() const override { return length; }
	U64 tell() const override { return position; }

	bool seek(U64 offset) override {
		if (offset > length) {
			failed = true;
			return false;
		}

		position = offset;
		return true;
	}

	size_t read(std::span<U8> dst) override {
		const U64 remaining = length - position;
		const size_t want = static_cast<size_t>(std::min<U64>(dst.size(), remaining));
		if (want == 0)
			return 0;

		std::memcpy(dst.data(), bytes + position, want);
		position += want;
		return want;
	}

	bool ok() const override { return !failed; }

private:
	const U8* bytes = nullptr;
	U64 length = 0;
	U64 position = 0;
	bool failed = false;
};

} // namespace Blackthorn::Assets
