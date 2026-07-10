#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Assets/PackMount.h"
#include "Core/Export.h"

namespace Blackthorn::Assets {

/**
 * @class AssetResolver
 * @brief Manages a priority-ordered stack of PackMount instances.
 *
 * In Release builds (BT_PACK_MODE defined), all asset reads go through
 * the resolver. In Debug builds it is a no-op and PathLoadParams is used
 * directly. Mod support is implemented by mounting additional .btp files
 * at runtime after initial game packs are mounted, last mounted wins.
 *
 * @section usage Usage
 * @code
 * // Engine startup, mount base pack first then DLC/mods on top.
 * resolver.mount("data/base.btp");
 * resolver.mount("data/chapter1.btp");
 *
 * // Mod loader mounts user packs last they win all conflicts.
 * resolver.mount("mods/my_skin_pack.btp");
 *
 * // Asset resolution.
 * auto result = resolver.resolve("player_texture");
 * if (result) { // result->bytes is ready to pass to the loader // }
 * @endcode
 */
class BLACKTHORN_API AssetResolver {
public:
	/**
	 * @brief Mounts a .btp file onto the priority stack.
	 *
	 * Last mounted = highest priority (last-wins semantics).
	 * Safe to call at any time, including after game startup for mods.
	 *
	 * @param path Path to the .btp file on disk.
	 * @return true if the pack mounted successfully.
	 */
	bool mount(const std::filesystem::path& path);

	/**
	 * @brief Unmounts a previously mounted pack file.
	 *
	 * Useful for unloading a chapter's pack after the player leaves that
	 * area, freeing the TOC memory.
	 *
	 * @param path Path that was passed to mount().
	 */
	void unmount(const std::filesystem::path& path);

	/**
	 * @brief Resolves an asset string ID to decompressed bytes.
	 *
	 * Searches the mount stack from highest priority to lowest.
	 *
	 * @param id The asset string ID (e.g. "player_texture").
	 * @return Decompressed asset data, or std::nullopt if not found.
	 */
	std::optional<PackedAssetData> resolve(const std::string& id) const;

	/// Returns true if any mounted pack contains the given asset ID.
	bool has(const std::string& id) const;

	size_t mountCount() const { return m_mounts.size(); }

private:
	/// xxHash64 of a string ID. Deterministic across platforms.
	static uint64_t hash(const std::string& id);

	/// Mounts in mount order. We search in reverse for last-wins.
	std::vector<PackMount> m_mounts;
};

} // namespace Blackthorn::Assets