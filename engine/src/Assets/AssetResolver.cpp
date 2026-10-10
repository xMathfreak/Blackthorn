#include "Assets/AssetResolver.h"

#include <algorithm>
#include <cstdio>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Debug/Logger.h"
#include "FileIO.h"

namespace Blackthorn::Assets {

namespace {

#ifndef BT_PACK_MODE
/**
 * @brief Reads a whole file into @p out. Returns false if it can't be opened or read.
 */
bool readLooseFile(const std::filesystem::path& path, std::vector<U8>& out) {
	std::FILE* f = std::fopen(path.string().c_str(), "rb");
	if (!f)
		return false;

	if (!Blackthorn::seekTo(f, 0, SEEK_END)) {
		std::fclose(f);
		return false;
	}

	const int64_t size = Blackthorn::fileTell(f);
	if (size < 0 || !Blackthorn::seekTo(f, 0, SEEK_SET)) {
		std::fclose(f);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const bool ok = Blackthorn::readExact(f, out.data(), out.size());
	std::fclose(f);
	return ok;
}

#endif

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
		if (!it->has(hash))
			continue;

		auto packed = it->read(hash);
		if (!packed) {
			BT_WARN("AssetResolver: read failed for '{}' in '{}', trying lower-priority mounts",
				*norm, it->path().string());
			continue;
		}

		AssetBytes out;
		out.bytes = std::move(packed->bytes);
		out.id = *norm;
		out.sourcePath = packed->sourcePath;
		return out;
	}

	BT_WARN("AssetResolver: '{}' not found in any mounted pack", *norm);
	return std::nullopt;
#else
	const std::filesystem::path path = looseRoot() / *norm;

	AssetBytes out;
	if (!readLooseFile(path, out.bytes)) {
		BT_WARN("AssetResolver: '{}' not found at '{}'", *norm, path.string());
		return std::nullopt;
	}

	out.id = *norm;
	out.sourcePath = path.string();
	return out;
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
		if (it->has(hash))
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
