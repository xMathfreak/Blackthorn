#include "Assets/AssetResolver.h"

#include <algorithm>
#include <cstdio>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Debug/Logger.h"
#include "FileIO.h"

namespace Blackthorn::Assets {

namespace {

/**
 * @brief Reads a whole stream into a fresh buffer. Fails on any short read or I/O error.
 */
std::optional<AssetBytes> readAllFrom(AssetStream& stream, const std::string& id, const std::string& sourcePath) {
	std::vector<U8> bytes(static_cast<size_t>(stream.size()));

	size_t total = 0;
	while (total < bytes.size()) {
		const size_t got = stream.read(std::span<U8>(bytes.data() + total, bytes.size() - total));
		if (got == 0)
			break;

		total += got;
	}

	if (!stream.ok() || total != bytes.size()) {
		BT_ERROR("AssetResolver: read failed for '{}' from '{}' ({} of {} bytes)",
			id, sourcePath, total, bytes.size());
		return std::nullopt;
	}

	AssetBytes out;
	out.bytes = std::move(bytes);
	out.id = id;
	out.sourcePath = sourcePath;
	return out;
}

} // namespace

void AssetResolver::setLooseRoot(const std::filesystem::path& root) {
	std::unique_lock lock(mutex);
	looseRootPath = root;
}

std::filesystem::path AssetResolver::looseRoot() const {
	std::shared_lock lock(mutex);
	return looseRootPath;
}

bool AssetResolver::mount(const std::filesystem::path& path) {
	std::unique_lock lock(mutex);

	for (const PackMount& m : mounts) {
		if (m.path() == path) {
			BT_WARN("AssetResolver: '{}' is already mounted, ignoring duplicate", path.string());
			return true;
		}
	}

	const U32 priority = static_cast<U32>(mounts.size());

	PackMount mount;
	if (!mount.mount(path, priority))
		return false;

	mounts.push_back(std::move(mount));
	return true;
}

void AssetResolver::unmount(const std::filesystem::path& path) {
	std::unique_lock lock(mutex);

	const auto it = std::find_if(
		mounts.begin(), mounts.end(),
		[&path](const PackMount& m) { return m.path() == path; }
	);

	if (it == mounts.end()) {
		BT_WARN("AssetResolver: unmount called for '{}' which is not mounted", path.string());
		return;
	}

	BT_LOG("AssetResolver: unmounting '{}' ({} asset(s))", path.string(), it->entryCount());
	mounts.erase(it);

	for (U32 i = 0; i < static_cast<U32>(mounts.size()); ++i)
		mounts[i].packPriority = i;
}

std::unique_ptr<AssetStream> AssetResolver::open(std::string_view id) const {
	const auto norm = Blackthorn::normalizeAssetPath(id);
	if (!norm) {
		BT_WARN("AssetResolver: invalid asset ID '{}'", std::string(id));
		return nullptr;
	}

#ifdef BT_PACK_MODE
	const U64 hash = XXH64(norm->data(), norm->size(), 0);

	std::shared_lock lock(mutex);

	for (auto it = mounts.rbegin(); it != mounts.rend(); ++it) {
		if (!it->find(hash))
			continue;

		// First match wins, even if it can't be streamed. A compressed entry in
		// a higher-priority pack must not fall through to a lower one.
		auto stream = it->open(hash);
		if (!stream)
			BT_ERROR("AssetResolver: '{}' in '{}' can't be streamed (compressed entry, or truncated pack)",
				*norm, it->path().string());

		return stream;
	}

	BT_WARN("AssetResolver: '{}' not found in any mounted pack", *norm);
	return nullptr;
#else
	const std::filesystem::path path = looseRoot() / *norm;

	auto stream = LooseAssetStream::open(path);
	if (!stream)
		BT_WARN("AssetResolver: '{}' not found at '{}'", *norm, path.string());

	return stream;
#endif
}

std::optional<AssetBytes> AssetResolver::resolve(std::string_view id) const {
	const auto norm = Blackthorn::normalizeAssetPath(id);
	if (!norm) {
		BT_WARN("AssetResolver: invalid asset ID '{}'", std::string(id));
		return std::nullopt;
	}

#ifdef BT_PACK_MODE
	const U64 hash = XXH64(norm->data(), norm->size(), 0);

	std::shared_lock lock(mutex);

	for (auto it = mounts.rbegin(); it != mounts.rend(); ++it) {
		const auto info = it->find(hash);
		if (!info)
			continue;

		if (info->compression == PackCompression::Zstd) {
			// PackMount::read() decompresses and verifies the payload hash itself.
			auto packed = it->read(hash);
			if (!packed) {
				BT_ERROR("AssetResolver: read failed for '{}' in '{}'", *norm, it->path().string());
				return std::nullopt;
			}

			AssetBytes out;
			out.bytes = std::move(packed->bytes);
			out.id = *norm;
			out.sourcePath = packed->sourcePath;
			return out;
		}

		// Uncompressed entries are read through the stream. Streaming skips the
		// payload hash, so verify it here while we have the whole payload.
		auto stream = it->open(hash);
		if (!stream) {
			BT_ERROR("AssetResolver: cannot open '{}' in '{}'", *norm, it->path().string());
			return std::nullopt;
		}

		auto bytes = readAllFrom(*stream, *norm, it->path().string());
		if (!bytes)
			return std::nullopt;

		const U64 actual = XXH64(bytes->bytes.data(), bytes->bytes.size(), 0);
		if (actual != info->payloadHash) {
			BT_ERROR("AssetResolver: payload hash mismatch for '{}' in '{}'", *norm, it->path().string());
			return std::nullopt;
		}

		return bytes;
	}

	BT_WARN("AssetResolver: '{}' not found in any mounted pack", *norm);
	return std::nullopt;
#else
	const std::filesystem::path path = looseRoot() / *norm;

	auto stream = LooseAssetStream::open(path);
	if (!stream) {
		BT_WARN("AssetResolver: '{}' not found at '{}'", *norm, path.string());
		return std::nullopt;
	}

	return readAllFrom(*stream, *norm, path.string());
#endif
}

std::optional<std::string> AssetResolver::resolveReference(std::string_view baseID, std::string_view ref) const {
	return Blackthorn::resolveAssetReference(baseID, ref);
}

bool AssetResolver::has(std::string_view id) const {
	const auto norm = Blackthorn::normalizeAssetPath(id);
	if (!norm)
		return false;

#ifdef BT_PACK_MODE
	const U64 hash = XXH64(norm->data(), norm->size(), 0);

	std::shared_lock lock(mutex);

	for (auto it = mounts.rbegin(); it != mounts.rend(); ++it) {
		if (it->find(hash))
			return true;
	}

	return false;
#else
	std::error_code ec;
	return std::filesystem::is_regular_file(looseRoot() / *norm, ec);
#endif
}

size_t AssetResolver::mountCount() const {
	std::shared_lock lock(mutex);
	return mounts.size();
}

std::optional<PackMetadata> AssetResolver::getPackMetadata(const std::filesystem::path& path) const {
	std::shared_lock lock(mutex);

	const auto it = std::find_if(
		mounts.begin(), mounts.end(),
		[&path](const PackMount& m) { return m.path() == path; }
	);

	if (it == mounts.end())
		return std::nullopt;

	return it->getMetadata();
}

} // namespace Blackthorn::Assets
