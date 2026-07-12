#pragma once

#include <filesystem>
#include <ostream>

#include "Manifest.h"

namespace BTPacker {

/**
 * @class Packer
 * @brief Core .btp file operations: pack, verify, list, and unpack.
 *
 * All methods are static; Packer holds no state. Results and progress are
 * written to the provided std::ostream (normally std::cout) so tests can
 * redirect output trivially.
 *
 * @section pack_order Pack write order
 * 1. Reserve 64 bytes for the BTPHeader (written last at offset 0).
 * 2. For each asset: read source file, compress with zstd, xxHash the
 *    compressed blob, write the blob. Record a BTPEntry.
 * 3. Serialize the BTPEntry array into a flat byte buffer, compress it as a
 *    single zstd block, write it. Record the TOC offset and size.
 * 4. (Optional) Write the uncompressed symbol table.
 * 5. Seek back to offset 0, write the finalised BTPHeader.
 *
 * Each data blob is independently compressed so any entry can be randomly
 * accessed and decompressed without touching the rest of the file.
 */
class Packer {
public:
	/**
	 * @brief Packs all assets listed in @p manifest into a single .btp file.
	 *
	 * Creates any missing parent directories for the output path.
	 * On failure, any partially-written output file is removed.
	 *
	 * @param manifest  Parsed PackManifest.
	 * @param out       Stream to write progress messages to.
	 * @return true on success.
	 */
	static bool pack(const PackManifest& manifest, std::ostream& out);

	/**
	 * @brief Verifies every entry in a .btp file by decompressing it and
	 *        checking the stored xxHash64 against the compressed bytes.
	 *
	 * @param btpPath Path to the .btp file.
	 * @param out     Stream to write results to.
	 * @return true if all entries pass; false if any fail or the file is invalid.
	 */
	static bool verify(const std::filesystem::path& btpPath, std::ostream& out);

	/**
	 * @brief Prints the table of contents of a .btp file.
	 *
	 * Prints the asset ID (hex), asset type, compression codec,
	 * uncompressed / compressed sizes and if a symbol table is present,
	 * the original string ID and source path for each entry.
	 *
	 * @param btpPath Path to the .btp file.
	 * @param out     Stream to write output to.
	 * @return true on success.
	 */
	static bool list(const std::filesystem::path& btpPath, std::ostream& out);

	/**
	 * @brief Decompresses every asset in a .btp file into a directory.
	 *
	 * Used for debugging. If a symbol table is present, each asset is written
	 * to a file named after its original source path (directory structure
	 * preserved under @p destDir). Otherwise, files are named by their hex
	 * asset ID.
	 *
	 * @param btpPath  Path to the .btp file.
	 * @param destDir  Directory to write decompressed assets into.
	 * @param out      Stream to write progress to.
	 * @return true on success.
	 */
	static bool unpack(
		const std::filesystem::path& btpPath,
		const std::filesystem::path& destDir,
		std::ostream& out
	);
};

} // namespace BTPacker