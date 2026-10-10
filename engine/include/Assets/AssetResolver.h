#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "AssetNormalize.h"
#include "AssetReference.h"
#include "Assets/PackMount.h"
#include "Core/Export.h"

namespace Blackthorn::Assets {

/**
 * @brief Bytes for one asset, plus the canonical ID they were requested under.
 *
 * The canonical ID is what loaders use as the base for resolving references
 * inside the asset's data. sourcePath is for diagnostics only.
 */
struct BLACKTHORN_API AssetBytes {
	std::vector<U8> bytes;
	std::string     id;
	std::string     sourcePath;
};

/**
 * @class AssetResolver
 * @brief Single entry point for reading asset bytes and expanding references.
 *
 * Every asset ID is a canonical path of the form root/path/to/file, where the
 * first segment is a top-level folder under the loose root. The same ID works
 * in both build modes:
 *
 * - Loose builds read `looseRoot() / id` from disk.
 * - BT_PACK_MODE builds search mounted packs, highest priority first. A miss
 *   is an error. There is no loose-file fallback, so missing content fails
 *   loudly instead of hiding a packing mistake.
 *
 * @section references References inside data files
 * Data files reference other assets using resolveReference():
 *   smoke.png              relative to the containing file's directory
 *   ../textures/x.png      relative, climbing up but never past the root
 *   @root/particles/x.png  absolute within the containing file's own root
 *   @engine/particles/x.png absolute in the named root "engine"
 *
 * @section mount_order Mount order (pack mode)
 * Mounts are searched in reverse insertion order, so the last pack mounted
 * wins any ID conflict. Base game packs mount first, DLC and mods mount last.
 *
 * @section thread_safety Thread safety
 * resolve(), has(), and resolveReference() are safe to call from any thread.
 * mount(), unmount(), and setLooseRoot() take an exclusive lock.
 */
class BLACKTHORN_API AssetResolver {
public:
	AssetResolver() = default;
	~AssetResolver() = default;

	AssetResolver(const AssetResolver&) = delete;
	AssetResolver& operator=(const AssetResolver&) = delete;

	/**
	 * @brief Sets the directory that loose-mode IDs are relative to.
	 *
	 * Defaults to the working directory at construction. Thread-safe.
	 *
	 * @param root Directory containing the top-level asset folders.
	 */
	void setLooseRoot(const std::filesystem::path& root);

	/// Current loose root.
	std::filesystem::path looseRoot() const;

	/**
	 * @brief Mounts a .btp file onto the top of the priority stack.
	 *
	 * Thread-safe. If the same path is already mounted, this is a no-op and
	 * returns true.
	 *
	 * @return true if the pack was mounted (or was already mounted).
	 */
	bool mount(const std::filesystem::path& path);

	/**
	 * @brief Unmounts a previously mounted pack file.
	 *
	 * Thread-safe. Assets already loaded from this pack stay in AssetStorage.
	 * If @p path is not mounted, this is a no-op.
	 */
	void unmount(const std::filesystem::path& path);

	/**
	 * @brief Reads the bytes for a canonical asset ID.
	 *
	 * @param id A canonical ID such as "assets/particles/smoke.png". The ID is
	 *           normalized before use, so "assets\\particles\\smoke.png" is
	 *           accepted and refers to the same asset.
	 * @return The bytes and canonical ID, or std::nullopt if the asset is not
	 *         found (not in any mounted pack in BT_PACK_MODE, not on disk
	 *         otherwise). Misses are logged.
	 */
	std::optional<AssetBytes> resolve(std::string_view id) const;

	/**
	 * @brief Resolves a reference written inside the asset with ID @p baseID.
	 *
	 * See the class documentation for the reference forms. This does not check
	 * that the target exists; resolve() does that.
	 *
	 * @return The canonical ID, or std::nullopt if the reference is invalid.
	 */
	std::optional<std::string> resolveReference(std::string_view baseID, std::string_view ref) const;

	/**
	 * @brief Returns true if the asset exists in the active mode's storage.
	 *
	 * Does not read or decompress the asset.
	 */
	bool has(std::string_view id) const;

	/// Number of currently mounted packs.
	size_t mountCount() const;

	/**
	 * @brief Returns the metadata of the pack mounted at @p path.
	 *
	 * @return The pack's PackMetadata, or std::nullopt if no pack is mounted
	 *         at @p path.
	 */
	std::optional<PackMetadata> getPackMetadata(const std::filesystem::path& path) const;

private:
	/// Mounts in insertion order. resolve() iterates in reverse for last-wins.
	std::vector<PackMount> mounts;

	/// Loose-mode root directory.
	std::filesystem::path looseRootPath = std::filesystem::current_path();

	/// Guards mounts and looseRootPath.
	mutable std::shared_mutex mutex;
};

} // namespace Blackthorn::Assets
