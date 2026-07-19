#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/PackFormat.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Assets {

/**
 * @brief Decompressed asset bytes returned by PackMount::read().
 *
 * Move-only. Owns the decompressed byte buffer so callers can pass it
 * directly to an IAsyncAssetLoader::upload() without any extra copy.
 */
struct BLACKTHORN_API PackedAssetData {
	std::vector<U8> bytes; ///< Decompressed asset bytes, ready to pass to a loader.
	PackAssetType assetType; ///< Broad category, used to dispatch to the right loader.
	std::string sourcePath; ///< Original source path. Empty in release builds.

	PackedAssetData() = default;
	PackedAssetData(PackedAssetData&&) noexcept = default;
	PackedAssetData& operator=(PackedAssetData&&) noexcept = default;

	PackedAssetData(const PackedAssetData&) = delete;
	PackedAssetData& operator=(const PackedAssetData&) = delete;
};

/**
 * @class PackMount
 * @brief Represents one mounted .btp pack file.
 *
 * On mount(), the file header is validated, the compressed TOC block is read
 * and decompressed, and the resulting BTPEntry array is indexed into an
 * unordered_map keyed by assetID for O(1) lookup. The TOC lives in RAM for
 * the lifetime of the mount. Individual asset data blobs are NOT loaded into
 * memory until read() is called for a specific asset. Assets are read, hash-
 * verified, and decompressed on demand.
 *
 * In debug builds, the symbol table (if present) is also loaded, mapping
 * each U64 assetID back to its original string ID and source path for
 * logging.
 *
 * @note read() opens the file, seeks, reads, and closes per call. This is
 * intentional: pack files are typically on an SSD, asset loads are already
 * async (JobSystem), and avoiding a persistent file handle sidesteps
 * platform-specific sharing / locking issues entirely.
 */
class BLACKTHORN_API PackMount {
public:
	PackMount() = default;
	~PackMount() = default;

	PackMount(PackMount&&) noexcept = default;
	PackMount& operator=(PackMount&&) noexcept = default;

	PackMount(const PackMount&) = delete;
	PackMount& operator=(const PackMount&) = delete;

	/**
	 * @brief Opens and validates a .btp file, loading its TOC into memory.
	 *
	 * @param path     Absolute or relative path to the .btp file.
	 * @param priority Mount priority. Higher values win over lower values
	 *                 when the resolver searches the stack. Callers normally
	 *                 pass the mount index so last-mounted has the highest value.
	 * @return true on success; false if the file cannot be opened, the magic
	 *         or version is wrong, or the TOC decompression fails.
	 */
	bool mount(const std::filesystem::path& path, U32 priority);

	/**
	 * @brief Returns true if this mount contains an entry for the given hashed ID.
	 *
	 * @param assetID xxHash64 of the asset string ID.
	 */
	bool has(U64 assetID) const;

	/**
	 * @brief Reads, integrity-checks, and decompresses the asset blob for assetID.
	 *
	 * Opens the backing .btp file, seeks to the blob offset recorded in the TOC,
	 * reads compressedSize bytes, verifies the xxHash64, decompresses with zstd
	 * into a pre-allocated buffer of uncompressedSize bytes, then closes the file.
	 *
	 * @param assetID xxHash64 of the asset string ID.
	 * @return PackedAssetData on success; std::nullopt if the asset is not found,
	 *         the hash check fails, or decompression fails.
	 */
	std::optional<PackedAssetData> read(U64 assetID) const;

	/// Path to the .btp file backing this mount.
	const std::filesystem::path& path() const { return packPath; }

	/// Priority value assigned at mount time (higher = wins conflicts).
	U32 priority() const { return packPriority; }

	/// Number of entries in the TOC.
	size_t entryCount() const { return contentMap.size(); }

	/// Returns true if mount() has been called successfully.
	bool isMounted() const { return mounted; }

private:
	// AssetResolver re-assigns priorities after an unmount to keep the
	// last-mounted-wins invariant intact. Granting friend access avoids
	// exposing a public setter that callers should never call directly.
	friend class AssetResolver;

	/**
	 * @brief Reads and decompresses the TOC block from the already-open file.
	 *
	 * @param file   Open FILE* positioned anywhere (will seek internally).
	 * @param header Validated BTPHeader read from offset 0.
	 * @return true on success.
	 */
	bool loadTOC(std::FILE* file, const BTPHeader& header);

#ifdef BLACKTHORN_DEBUG
	/**
	 * @brief Reads the debug symbol table if present.
	 *
	 * Populates symbols and sources. No-op if symbolTableOff is 0.
	 *
	 * @param file   Open FILE* positioned anywhere (will seek internally).
	 * @param header Validated BTPHeader read from offset 0.
	 */
	void loadSymbolTable(std::FILE* file, const BTPHeader& header);
#endif

	std::filesystem::path packPath;
	U32 packPriority = 0;
	bool mounted  = false;

	/// TOC indexed by assetID (xxHash64) for O(1) lookup.
	std::unordered_map<U64, BTPEntry> contentMap;

#ifdef BLACKTHORN_DEBUG
	/// assetID → original string ID (e.g. "player_texture").
	std::unordered_map<U64, std::string> symbols;

	/// assetID → source path recorded by the packer (e.g. "assets/textures/player.png").
	std::unordered_map<U64, std::string> sources;
#endif
};

} // namespace Blackthorn::Assets
