#include "Assets/AssetResolver.h"

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Debug/Logger.h"

namespace Blackthorn::Assets {

U64 AssetResolver::hashID(const std::string& id) {
	return XXH64(id.data(), id.size(), 0);
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

	for (U32 i = 0; i < static_cast<U32>(mounts.size()); ++i) {
		mounts[i].packPriority = i;
	}
}

std::optional<PackedAssetData> AssetResolver::resolve(const std::string& id) const {
	const U64 hash = hashID(id);
	std::shared_lock lock(mutex);

	for (auto it = mounts.rbegin(); it != mounts.rend(); ++it) {
		if (it->has(hash)) {
			auto data = it->read(hash);
			if (data) {
#ifdef BLACKTHORN_DEBUG
				BT_DEBUG(
					"AssetResolver: resolved '{}' from '{}'",
					id, it->path().string()
				);
#endif
				return data;
			}

			BT_WARN(
				"AssetResolver: read failed for '{}' in '{}', trying lower-priority mounts",
				id, it->path().string()
			);
		}
	}

	BT_WARN("AssetResolver: '{}' not found in any mounted pack", id);
	return std::nullopt;
}

bool AssetResolver::has(const std::string& id) const {
	const U64 hash = hashID(id);

	std::shared_lock lock(mutex);

	for (auto it = mounts.rbegin(); it != mounts.rend(); ++it) {
		if (it->has(hash))
			return true;
	}

	return false;
}

size_t AssetResolver::mountCount() const {
	std::shared_lock lock(mutex);
	return mounts.size();
}

} // namespace Blackthorn::Assets