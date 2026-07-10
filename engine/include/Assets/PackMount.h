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
 * Owns its buffer. Move-only so the bytes are never copied.
 */
struct PackedAssetData {
	std::vector<U8> bytes;
	std::string sourcePath;
	PackAssetType type;
};

/**
 * @class PackMount
 * @brief Represents one mounted .btp pack file.
 *
 * Holds the TOC in memory after mount() is called. Individual asset
 * blobs are read and decompressed on demand via read().
 *
 * Thread-safety: read() is thread-safe (shared file handle with mutex,
 * or per-call fopen). TOC lookup is read-only after mount().
 */
class BLACKTHORN_API PackMount {
public:
	/**
	 * @brief Mounts a .btp file, loading and decompressing its TOC.
	 *
	 * @param path Path to the .btp file.
	 * @param priority Explicit priority (higher = wins). When using
	 * last-mounted-wins, pass the mount index.
	 * @return true on success, false if the file is invalid.
	 */
	bool mount(const std::filesystem::path& path, uint32_t priority);

	/// Returns true if this mount contains an entry for @p assetID.
	bool has(uint64_t assetID) const;

	/**
	 * @brief Reads, verifies, and decompresses the asset with the given ID.
	 *
	 * @param assetID xxHash64 of the asset string ID.
	 * @return Decompressed bytes, or std::nullopt on failure.
	 */
	std::optional<PackedAssetData> read(uint64_t assetID) const;

	/// Path to the .btp file this mount represents.
	const std::filesystem::path& path() const { return m_path; }

	uint32_t priority() const { return m_priority; }

private:
	std::filesystem::path m_path;
	uint32_t m_priority = 0;
	std::unordered_map<uint64_t, BTPEntry> m_toc;

#ifdef BLACKTHORN_DEBUG
	std::unordered_map<uint64_t, std::string> m_symbols; ///< ID -> original string.
	std::unordered_map<uint64_t, std::string> m_sources; ///< ID -> source path.
#endif
};

} // namespace Blackthorn::Assets