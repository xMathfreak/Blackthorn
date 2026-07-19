#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Assets/PackMount.h"
#include "Core/Export.h"

namespace Blackthorn::Assets {

/**
 * @class AssetResolver
 * @brief Priority-ordered stack of PackMount instances.
 *
 * The resolver is the single point through which all pack-mode asset reads
 * flow. It maintains an ordered list of PackMount objects and searches them
 * from highest priority to lowest (last-mounted wins) when resolve() is
 * called.
 *
 * @section mount_order Mount order and priority
 * Mounts are assigned a monotonically increasing priority equal to their
 * insertion index. The search in resolve() iterates the vector in reverse,
 * so the last mounted pack wins any ID conflict. This matches Steam Workshop
 * semantics: base game packs are mounted first, DLC on top of those, and
 * user mod packs last.
 *
 * @code
 * // Engine startup
 * resolver.mount("data/base.btp");       // priority 0 (lowest)
 * resolver.mount("data/chapter1.btp");   // priority 1
 *
 * // DLC installed
 * resolver.mount("dlc/skin_pack.btp");   // priority 2 (wins over base/chapter1)
 *
 * // User mod loaded at runtime
 * resolver.mount("mods/my_mod.btp");     // priority 3 (highest, wins everything)
 * @endcode
 *
 * @section thread_safety Thread safety
 * resolve() and has() acquire a shared lock and are safe to call from any
 * thread concurrently (e.g. from decode jobs on the JobSystem).
 * mount() and unmount() acquire an exclusive lock and may block briefly while
 * readers finish, but do not stall the job system for long.
 */
class BLACKTHORN_API AssetResolver {
public:
	AssetResolver() = default;
	~AssetResolver() = default;

	AssetResolver(const AssetResolver&) = delete;
	AssetResolver& operator=(const AssetResolver&) = delete;

	/**
	 * @brief Mounts a .btp file onto the top of the priority stack.
	 *
	 * Thread-safe. Safe to call at any time, including after game startup
	 * for runtime mod or DLC support. If the same path is already mounted,
	 * this is a no-op and returns true.
	 *
	 * @param path Absolute or relative path to the .btp file.
	 * @return true if the pack was mounted (or was already mounted);
	 *         false if the file could not be opened or validated.
	 */
	bool mount(const std::filesystem::path& path);

	/**
	 * @brief Unmounts a previously mounted pack file.
	 *
	 * Thread-safe. Removes the PackMount from the stack and frees its TOC
	 * memory. Assets already loaded from this pack remain alive in
	 * AssetStorage, they are not evicted. If @p path is not currently
	 * mounted, this is a no-op.
	 *
	 * Typical use: unmounting a chapter pack after the player has left that
	 * area to recover the TOC RAM.
	 *
	 * @param path Path that was previously passed to mount().
	 */
	void unmount(const std::filesystem::path& path);

	/**
	 * @brief Resolves a string asset ID to decompressed bytes.
	 *
	 * Hashes @p id with xxHash64, then searches the mount stack from highest
	 * priority to lowest, returning the first hit. Returns std::nullopt if no
	 * mounted pack contains the asset.
	 *
	 * Thread-safe (shared lock).
	 *
	 * @param id The asset string ID (e.g. "player_texture").
	 * @return Decompressed PackedAssetData, or std::nullopt if not found.
	 */
	std::optional<PackedAssetData> resolve(const std::string& id) const;

	/**
	 * @brief Returns true if any mounted pack contains the given asset ID.
	 *
	 * Thread-safe (shared lock). Does NOT decompress, only checks the TOC.
	 *
	 * @param id The asset string ID.
	 */
	bool has(const std::string& id) const;

	/// Number of currently mounted packs.
	size_t mountCount() const;

private:
	/**
	 * @brief Computes the xxHash64 of a string asset ID.
	 *
	 * Deterministic across platforms and build configurations. Both the packer
	 * and the resolver must use the same seed (0) so IDs match.
	 *
	 * @param id The asset string ID.
	 * @return 64-bit hash used as the binary key in BTPEntry::assetID.
	 */
	static uint64_t hashID(const std::string& id);

	/// Mounts in insertion order. resolve() iterates in reverse for last-wins.
	std::vector<PackMount> mounts;

	/// Guards mounts. resolve()/has() take shared; mount()/unmount() take exclusive.
	mutable std::shared_mutex mutex;
};

} // namespace Blackthorn::Assets