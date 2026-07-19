#include "Packer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <zstd.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Assets/PackFormat.h"

using namespace Blackthorn::Assets;

namespace BTPacker {

namespace {

/// Seeks an open FILE* to an absolute byte offset. Returns false on failure.
bool seekTo(std::FILE* f, uint64_t offset) {
#ifdef _WIN32
	return _fseeki64(f, static_cast<__int64>(offset), SEEK_SET) == 0;
#else
	return std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0;
#endif
}

/// Returns the current byte position of an open FILE*. Returns -1 on failure.
int64_t fileTell(std::FILE* f) {
#ifdef _WIN32
	return _ftelli64(f);
#else
	return static_cast<int64_t>(std::ftell(f));
#endif
}

/// Reads exactly @p count bytes from @p f into @p dst. Returns false on short read.
bool readExact(std::FILE* f, void* dst, size_t count) {
	return std::fread(dst, 1, count, f) == count;
}

/// Writes exactly @p count bytes from @p src into @p f. Returns false on error.
bool writeExact(std::FILE* f, const void* src, size_t count) {
	return std::fwrite(src, 1, count, f) == count;
}

/**
 * @brief Reads the entire contents of a file into a vector.
 * @return true on success; false if the file cannot be opened or read.
 */
bool readFile(const std::filesystem::path& path, std::vector<uint8_t>& out) {
	std::FILE* f = std::fopen(path.string().c_str(), "rb");
	if (!f) {
		std::cerr << "btpacker: error: cannot open source file '" << path.string() << "'\n";
		return false;
	}

	std::fseek(f, 0, SEEK_END);
	const long size = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);

	if (size < 0) {
		std::fclose(f);
		std::cerr << "btpacker: error: cannot determine size of '" << path.string() << "'\n";
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const bool ok = readExact(f, out.data(), out.size());
	std::fclose(f);

	if (!ok) {
		std::cerr << "btpacker: error: short read from '" << path.string() << "'\n";
		return false;
	}

	return true;
}

/**
 * @brief Compresses @p src with zstd at @p level into @p out.
 * @return true on success.
 */
bool compressZstd(
	const std::vector<uint8_t>& src,
	std::vector<uint8_t>& out,
	int level
) {
	const size_t bound = ZSTD_compressBound(src.size());
	out.resize(bound);

	const size_t result = ZSTD_compress(
		out.data(), bound,
		src.data(), src.size(),
		level
	);

	if (ZSTD_isError(result)) {
		std::cerr << "btpacker: zstd compress error: " << ZSTD_getErrorName(result) << "\n";
		return false;
	}

	out.resize(result);
	return true;
}

/**
 * @brief Decompresses one zstd blob into a vector sized to @p uncompressedSize.
 * @return true on success.
 */
bool decompressZstd(
	const std::vector<uint8_t>& src,
	std::vector<uint8_t>& out,
	uint64_t uncompressedSize
) {
	out.resize(static_cast<size_t>(uncompressedSize));
	const size_t result = ZSTD_decompress(
		out.data(), out.size(),
		src.data(), src.size()
	);

	if (ZSTD_isError(result)) {
		std::cerr << "btpacker: zstd decompress error: " << ZSTD_getErrorName(result) << "\n";
		return false;
	}

	return true;
}

/**
 * @brief Maps a type string from the manifest to the PackAssetType enum.
 *
 * Case-insensitive. Unknown strings map to PackAssetType::Raw with a warning.
 */
PackAssetType resolveAssetType(const std::string& typeStr) {
	std::string lower = typeStr;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c){ return static_cast<char>(std::tolower(c)); }
	);

	if (lower == "texture")
		return PackAssetType::Texture;
	if (lower == "audio")
		return PackAssetType::Audio;
	if (lower == "shader")
		return PackAssetType::Shader;
	if (lower == "font")
		return PackAssetType::Font;
	if (lower == "raw")
		return PackAssetType::Raw;

	std::cerr << "btpacker: warning: unknown asset type '" << typeStr << "', treating as Raw\n";
	return PackAssetType::Raw;
}

const char* assetTypeName(PackAssetType t) {
	switch (t) {
		case PackAssetType::Texture:
			return "Texture";
		case PackAssetType::Audio:
			return "Audio";
		case PackAssetType::Shader:
			return "Shader";
		case PackAssetType::Font:
			return "Font";
		case PackAssetType::Raw:
			return "Raw";
		default:
			return "Unknown";
	}
}

const char* compressionName(PackCompression c) {
	switch (c) {
		case PackCompression::Zstd:
			return "zstd";
		case PackCompression::None:
			return "none";
		default:
			return "?";
	}
}

/**
 * @brief Reads and validates the BTPHeader from the beginning of @p f.
 * @return true on success; emits errors to stderr on failure.
 */
bool readHeader(std::FILE* f, BTPHeader& header, const std::string& filePath) {
	if (!seekTo(f, 0)) {
		std::cerr << "btpacker: error: seek failed in '" << filePath << "'\n";
		return false;
	}

	if (!readExact(f, &header, sizeof(BTPHeader))) {
		std::cerr << "btpacker: error: cannot read header from '" << filePath << "'\n";
		return false;
	}

	if (header.magic != BTP_MAGIC) {
		std::cerr << "btpacker: error: '" << filePath << "' is not a .btp file (bad magic)\n";
		return false;
	}

	if (header.version != BTP_VERSION) {
		std::cerr << "btpacker: error: '" << filePath
				  << "' uses version " << header.version
				  << " (expected " << BTP_VERSION << ")\n";

		return false;
	}

	return true;
}

/**
 * @brief Reads and decompresses the TOC from an already-open, validated pack file.
 * @return true on success.
 */
bool readTOC(
	std::FILE* f,
	const BTPHeader& header,
	const std::string& filePath,
	std::vector<BTPEntry>& entries
) {
	if (!seekTo(f, header.tocOffset)) {
		std::cerr << "btpacker: error: seek to TOC failed in '" << filePath << "'\n";
		return false;
	}

	std::vector<uint8_t> compressedTOC(static_cast<size_t>(header.tocCompSize));
	if (!readExact(f, compressedTOC.data(), compressedTOC.size())) {
		std::cerr << "btpacker: error: cannot read TOC from '" << filePath << "'\n";
		return false;
	}

	std::vector<uint8_t> rawTOC;
	if (!decompressZstd(compressedTOC, rawTOC, header.tocUncompSize))
		return false;

	if (rawTOC.size() % sizeof(BTPEntry) != 0) {
		std::cerr << "btpacker: error: decompressed TOC size is not a multiple of BTPEntry\n";
		return false;
	}

	const size_t count = rawTOC.size() / sizeof(BTPEntry);
	entries.resize(count);
	std::memcpy(entries.data(), rawTOC.data(), rawTOC.size());
	return true;
}

/**
 * @brief Reads the symbol table from an open file into two maps.
 *
 * Populates @p symbols (assetID → string ID) and @p sources (assetID → relative source path).
 * No-op if header.symbolTableOff is 0.
 */
void readSymbolTable(
	std::FILE* f,
	const BTPHeader& header,
	std::unordered_map<uint64_t, std::string>& symbols,
	std::unordered_map<uint64_t, std::string>& sources
) {
	if (header.symbolTableOff == 0 || header.symbolTableSize == 0)
		return;

	if (!seekTo(f, header.symbolTableOff))
		return;

	std::vector<uint8_t> block(static_cast<size_t>(header.symbolTableSize));
	if (!readExact(f, block.data(), block.size()))
		return;

	const uint8_t* cursor = block.data();
	const uint8_t* end = block.data() + block.size();

	while (cursor < end) {
		if (cursor + sizeof(uint64_t) + sizeof(uint16_t) > end)
			break;

		uint64_t assetID = 0;
		std::memcpy(&assetID, cursor, sizeof(uint64_t));
		cursor += sizeof(uint64_t);

		uint16_t idLen = 0;
		std::memcpy(&idLen, cursor, sizeof(uint16_t));
		cursor += sizeof(uint16_t);

		if (cursor + idLen > end)
			break;

		std::string idStr(reinterpret_cast<const char*>(cursor), idLen);
		cursor += idLen;

		const uint8_t* pathStart = cursor;
		while (cursor < end && *cursor != '\0')
			++cursor;

		std::string srcPath(reinterpret_cast<const char*>(pathStart),
							static_cast<size_t>(cursor - pathStart));

		if (cursor < end)
			++cursor;

		symbols[assetID] = std::move(idStr);
		sources[assetID] = std::move(srcPath);
	}
}

/**
 * @brief Appends one entry to the in-memory symbol table byte buffer.
 *
 * Binary layout per entry:
 *   uint64_t  assetID
 *   uint16_t  idLen
 *   char      id[idLen]          (not null-terminated)
 *   char      relSourcePath[]    (null-terminated, relative to manifest dir)
 *
 * Storing a relative path means the symbol table is portable across machines
 * and does not leak local directory structure into shipped binaries.
 *
 * @param buf         Buffer to append to.
 * @param assetID     xxHash64 of the asset string ID.
 * @param id          The asset string ID (e.g. "player_tex").
 * @param absPath     Absolute path to the source file on disk.
 * @param manifestDir Directory of the manifest; used to compute the relative path.
 */
void appendSymbolEntry(
	std::vector<uint8_t>& buf,
	uint64_t assetID,
	const std::string& id,
	const std::filesystem::path& absPath,
	const std::filesystem::path& manifestDir
) {
	std::string relPath;
	std::error_code ec;
	const auto rel = std::filesystem::relative(absPath, manifestDir, ec);

	if (!ec && !rel.empty()) {
		std::string raw = rel.generic_string();
		relPath = std::move(raw);
	} else {
		relPath = absPath.generic_string();
	}

	// -- assetID (8 bytes) --
	const size_t idStart = buf.size();
	buf.resize(idStart + sizeof(uint64_t));
	std::memcpy(buf.data() + idStart, &assetID, sizeof(uint64_t));

	// -- idLen (2 bytes) --
	const uint16_t idLen = static_cast<uint16_t>(id.size());
	const size_t   idLenStart = buf.size();
	buf.resize(idLenStart + sizeof(uint16_t));
	std::memcpy(buf.data() + idLenStart, &idLen, sizeof(uint16_t));

	// -- id string (idLen bytes) --
	const size_t idStrStart = buf.size();
	buf.resize(idStrStart + idLen);
	std::memcpy(buf.data() + idStrStart, id.data(), idLen);

	// -- relative source path (null-terminated) --
	const size_t pathStart = buf.size();
	buf.resize(pathStart + relPath.size() + 1);
	std::memcpy(buf.data() + pathStart, relPath.data(), relPath.size());
	buf[pathStart + relPath.size()] = '\0';
}

} // anonymous namespace

bool Packer::pack(const PackManifest& manifest, std::ostream& log) {
	const auto outDir = manifest.outputPath.parent_path();

	if (!outDir.empty() && !std::filesystem::exists(outDir)) {
		std::error_code ec;
		std::filesystem::create_directories(outDir, ec);

		if (ec) {
			std::cerr << "btpacker: error: cannot create output directory '"
					  << outDir.string() << "': " << ec.message() << "\n";

			return false;
		}
	}

	const std::string outPathStr = manifest.outputPath.string();
	std::FILE* out = std::fopen(outPathStr.c_str(), "wb");

	if (!out) {
		std::cerr << "btpacker: error: cannot create output file '" << outPathStr << "'\n";
		return false;
	}

	BTPHeader header{};
	if (!writeExact(out, &header, sizeof(BTPHeader))) {
		std::cerr << "btpacker: error: cannot write header placeholder\n";
		std::fclose(out);
		std::filesystem::remove(manifest.outputPath);
		return false;
	}

	std::vector<BTPEntry> toc;
	toc.reserve(manifest.assets.size());

	std::vector<uint8_t> symbolTableBytes;

	uint64_t totalSourceBytes = 0;
	uint64_t totalCompressedBytes = 0;

	for (const ManifestAsset& asset : manifest.assets) {
		if (!std::filesystem::exists(asset.sourcePath)) {
			std::cerr << "btpacker: error: source file not found: '"
					  << asset.sourcePath.string() << "' (asset '" << asset.id << "')\n";

			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}

		std::vector<uint8_t> raw;
		if (!readFile(asset.sourcePath, raw)) {
			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}

		std::vector<uint8_t> compressed;
		if (!compressZstd(raw, compressed, manifest.compressionLevel)) {
			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}

		const uint64_t blobHash = XXH64(compressed.data(), compressed.size(), 0);
		const uint64_t assetID = XXH64(asset.id.data(), asset.id.size(), 0);

		const int64_t dataOffset = fileTell(out);
		if (dataOffset < 0) {
			std::cerr << "btpacker: error: ftell failed while writing '" << asset.id << "'\n";
			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}

		if (!writeExact(out, compressed.data(), compressed.size())) {
			std::cerr << "btpacker: error: write failed for asset '" << asset.id << "'\n";
			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}

		BTPEntry entry{};
		entry.assetID = assetID;
		entry.dataOffset = static_cast<uint64_t>(dataOffset);
		entry.compressedSize = static_cast<uint64_t>(compressed.size());
		entry.uncompressedSize = static_cast<uint64_t>(raw.size());
		entry.xxhash = blobHash;
		entry.assetType = resolveAssetType(asset.typeStr);
		entry.compression = PackCompression::Zstd;
		toc.push_back(entry);

		if (manifest.writeSymbolTable) {
			appendSymbolEntry(
				symbolTableBytes,
				assetID,
				asset.id,
				asset.sourcePath,
				manifest.manifestDir
			);
		}

		const float ratio = raw.empty() ? 0.0f
			: (1.0f - static_cast<float>(compressed.size())
				/ static_cast<float>(raw.size())) * 100.0f;

		log << "  packed  " << asset.id
			<< "  [" << assetTypeName(entry.assetType) << "]"
			<< "  " << raw.size() << " B  ->  " << compressed.size() << " B"
			<< "  (" << std::fixed << std::setprecision(1) << ratio << "% smaller)\n";

		totalSourceBytes += raw.size();
		totalCompressedBytes += compressed.size();
	}

	const uint64_t tocOffset = static_cast<uint64_t>(fileTell(out));

	const size_t tocRawSize = toc.size() * sizeof(BTPEntry);
	std::vector<uint8_t> tocRaw(tocRawSize);
	std::memcpy(tocRaw.data(), toc.data(), tocRawSize);

	std::vector<uint8_t> tocCompressed;
	if (!compressZstd(tocRaw, tocCompressed, manifest.compressionLevel)) {
		std::fclose(out);
		std::filesystem::remove(manifest.outputPath);
		return false;
	}

	if (!writeExact(out, tocCompressed.data(), tocCompressed.size())) {
		std::cerr << "btpacker: error: write failed for TOC\n";
		std::fclose(out);
		std::filesystem::remove(manifest.outputPath);
		return false;
	}

	uint64_t symbolTableOff = 0;
	uint64_t symbolTableSize = 0;

	if (manifest.writeSymbolTable && !symbolTableBytes.empty()) {
		symbolTableOff = static_cast<uint64_t>(fileTell(out));
		symbolTableSize = static_cast<uint64_t>(symbolTableBytes.size());

		if (!writeExact(out, symbolTableBytes.data(), symbolTableBytes.size())) {
			std::cerr << "btpacker: error: write failed for symbol table\n";
			std::fclose(out);
			std::filesystem::remove(manifest.outputPath);
			return false;
		}
	}

	header.magic = BTP_MAGIC;
	header.version = BTP_VERSION;
	header.flags = 0;
	header.entryCount = static_cast<uint32_t>(toc.size());
	header.tocOffset = tocOffset;
	header.tocCompSize = static_cast<uint64_t>(tocCompressed.size());
	header.tocUncompSize = static_cast<uint64_t>(tocRawSize);
	header.symbolTableOff = symbolTableOff;
	header.symbolTableSize = symbolTableSize;
	std::memset(header.reserved, 0, sizeof(header.reserved));

	if (!seekTo(out, 0) || !writeExact(out, &header, sizeof(BTPHeader))) {
		std::cerr << "btpacker: error: failed to patch header\n";
		std::fclose(out);
		std::filesystem::remove(manifest.outputPath);
		return false;
	}

	std::fclose(out);

	const float overallRatio = (totalSourceBytes == 0) ? 0.0f
		: (1.0f - static_cast<float>(totalCompressedBytes)
			/ static_cast<float>(totalSourceBytes)) * 100.0f;

	const auto fileSize = std::filesystem::file_size(manifest.outputPath);

	log << "\n"
		<< "  output:       " << outPathStr << "\n"
		<< "  assets:       " << toc.size() << "\n"
		<< "  source size:  " << totalSourceBytes << " B\n"
		<< "  pack size:    " << fileSize << " B\n"
		<< "  reduction:    " << std::fixed << std::setprecision(1)
		<< overallRatio << "%\n"
		<< "  symbol table: " << (manifest.writeSymbolTable ? "yes" : "no")     << "\n";

	return true;
}

bool Packer::verify(const std::filesystem::path& btpPath, std::ostream& log) {
	const std::string pathStr = btpPath.string();
	std::FILE* f = std::fopen(pathStr.c_str(), "rb");
	if (!f) {
		std::cerr << "btpacker: error: cannot open '" << pathStr << "'\n";
		return false;
	}

	BTPHeader header{};
	if (!readHeader(f, header, pathStr)) {
		std::fclose(f);
		return false;
	}

	std::vector<BTPEntry> entries;
	if (!readTOC(f, header, pathStr, entries)) {
		std::fclose(f);
		return false;
	}

	std::unordered_map<uint64_t, std::string> symbols, sources;
	readSymbolTable(f, header, symbols, sources);

	log << "verifying '" << pathStr << "' (" << entries.size() << " entries)...\n";

	int passed = 0;
	int failed = 0;

	for (const BTPEntry& entry : entries) {
		if (!seekTo(f, entry.dataOffset)) {
			std::cerr << "  FAIL  0x" << std::hex << entry.assetID
					  << " - seek error\n" << std::dec;
			++failed;
			continue;
		}

		std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressedSize));
		if (!readExact(f, compressed.data(), compressed.size())) {
			std::cerr << "  FAIL  0x" << std::hex << entry.assetID
					  << " - read error\n" << std::dec;

			++failed;
			continue;
		}

		const uint64_t actualHash = XXH64(compressed.data(), compressed.size(), 0);
		if (actualHash != entry.xxhash) {
			std::cerr << "  FAIL  ";
			auto it = symbols.find(entry.assetID);

			if (it != symbols.end()) {
				std::cerr << it->second;
			} else {
				std::cerr << "0x" << std::hex << entry.assetID << std::dec;
			}

			std::cerr << " - hash mismatch (expected 0x" << std::hex
					  << entry.xxhash << ", got 0x" << actualHash << ")\n" << std::dec;

			++failed;
			continue;
		}

		if (entry.compression == PackCompression::Zstd) {
			std::vector<uint8_t> raw;
			if (!decompressZstd(compressed, raw, entry.uncompressedSize)) {
				std::cerr << "  FAIL  0x" << std::hex << entry.assetID
						  << " - decompression error\n" << std::dec;

				++failed;
				continue;
			}
		}

		auto it = symbols.find(entry.assetID);
		const std::string label = (it != symbols.end())
			? it->second
			: [&]{ std::ostringstream ss; ss << "0x" << std::hex << entry.assetID; return ss.str(); }();

		log << "  ok    " << label << "\n";
		++passed;
	}

	std::fclose(f);

	log << "\n  passed: " << passed << " / " << (passed + failed) << "\n";

	return failed == 0;
}

bool Packer::list(const std::filesystem::path& btpPath, std::ostream& log) {
	const std::string pathStr = btpPath.string();
	std::FILE* f = std::fopen(pathStr.c_str(), "rb");
	if (!f) {
		std::cerr << "btpacker: error: cannot open '" << pathStr << "'\n";
		return false;
	}

	BTPHeader header{};
	if (!readHeader(f, header, pathStr)) {
		std::fclose(f);
		return false;
	}

	std::vector<BTPEntry> entries;
	if (!readTOC(f, header, pathStr, entries)) {
		std::fclose(f);
		return false;
	}

	std::unordered_map<uint64_t, std::string> symbols, sources;
	readSymbolTable(f, header, symbols, sources);
	std::fclose(f);

	const auto fileSize = std::filesystem::file_size(btpPath);

	log << "pack: " << pathStr << "\n"
		<< "  version:      " << header.version << "\n"
		<< "  entries:      " << header.entryCount << "\n"
		<< "  file size:    " << fileSize << " B\n"
		<< "  symbol table: " << (header.symbolTableOff != 0 ? "yes" : "no") << "\n"
		<< "\n";

	log << std::left
		<< std::setw(20) << "asset ID (hex)"
		<< std::setw(10) << "type"
		<< std::setw(8)  << "codec"
		<< std::setw(14) << "raw (B)"
		<< std::setw(14) << "packed (B)"
		<< "string ID / source path\n";
	log << std::string(90, '-') << "\n";

	for (const BTPEntry& entry : entries) {
		std::ostringstream idHex;
		idHex << "0x" << std::hex << std::setw(16) << std::setfill('0') << entry.assetID;

		log << std::left << std::setfill(' ')
			<< std::setw(20) << idHex.str()
			<< std::setw(10) << assetTypeName(entry.assetType)
			<< std::setw(8)  << compressionName(entry.compression)
			<< std::setw(14) << entry.uncompressedSize
			<< std::setw(14) << entry.compressedSize;

		auto symIt = symbols.find(entry.assetID);
		auto srcIt = sources.find(entry.assetID);

		if (symIt != symbols.end())
			log << symIt->second;

		if (srcIt != sources.end() && !srcIt->second.empty())
			log << "  (" << srcIt->second << ")";

		log << "\n";
	}

	return true;
}

bool Packer::unpack(
	const std::filesystem::path& btpPath,
	const std::filesystem::path& destDir,
	std::ostream& log
) {
	const std::string pathStr = btpPath.string();
	std::FILE* f = std::fopen(pathStr.c_str(), "rb");
	if (!f) {
		std::cerr << "btpacker: error: cannot open '" << pathStr << "'\n";
		return false;
	}

	BTPHeader header{};
	if (!readHeader(f, header, pathStr)) {
		std::fclose(f);
		return false;
	}

	std::vector<BTPEntry> entries;
	if (!readTOC(f, header, pathStr, entries)) {
		std::fclose(f);
		return false;
	}

	std::unordered_map<uint64_t, std::string> symbols, sources;
	readSymbolTable(f, header, symbols, sources);

	std::error_code ec;
	std::filesystem::create_directories(destDir, ec);
	if (ec) {
		std::cerr << "btpacker: error: cannot create '" << destDir.string()
				  << "': " << ec.message() << "\n";
		std::fclose(f);
		return false;
	}

	log << "unpacking '" << pathStr << "' -> '" << destDir.string() << "'\n";

	bool anyFailed = false;

	for (const BTPEntry& entry : entries) {
		if (!seekTo(f, entry.dataOffset)) {
			std::cerr << "btpacker: error: seek failed for entry 0x"
					  << std::hex << entry.assetID << std::dec << "\n";
			anyFailed = true;
			continue;
		}

		std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressedSize));
		if (!readExact(f, compressed.data(), compressed.size())) {
			std::cerr << "btpacker: error: read failed for entry 0x"
					  << std::hex << entry.assetID << std::dec << "\n";

			anyFailed = true;
			continue;
		}

		std::filesystem::path outFile;
		auto srcIt = sources.find(entry.assetID);
		if (srcIt != sources.end() && !srcIt->second.empty()) {
			outFile = destDir / srcIt->second;
		} else {
			std::ostringstream name;
			name << std::hex << std::setw(16) << std::setfill('0') << entry.assetID << ".bin";
			outFile = destDir / name.str();
		}

		std::filesystem::create_directories(outFile.parent_path(), ec);
		if (ec) {
			std::cerr << "btpacker: error: cannot create directory '"
					  << outFile.parent_path().string() << "'\n";

			anyFailed = true;
			continue;
		}

		std::vector<uint8_t> raw;
		if (entry.compression == PackCompression::Zstd) {
			if (!decompressZstd(compressed, raw, entry.uncompressedSize)) {
				anyFailed = true;
				continue;
			}
		} else {
			raw = std::move(compressed);
		}

		std::FILE* outF = std::fopen(outFile.string().c_str(), "wb");
		if (!outF) {
			std::cerr << "btpacker: error: cannot create '" << outFile.string() << "'\n";
			anyFailed = true;
			continue;
		}

		const bool wrote = writeExact(outF, raw.data(), raw.size());
		std::fclose(outF);

		if (!wrote) {
			std::cerr << "btpacker: error: write failed for '" << outFile.string() << "'\n";
			anyFailed = true;
			continue;
		}

		auto symIt = symbols.find(entry.assetID);
		const std::string label = (symIt != symbols.end())
			? symIt->second
			: [&]{ std::ostringstream s; s << "0x" << std::hex << entry.assetID; return s.str(); }();

		log << "  unpacked  " << label << "  ->  " << outFile.string() << "\n";
	}

	std::fclose(f);
	return !anyFailed;
}

} // namespace BTPacker