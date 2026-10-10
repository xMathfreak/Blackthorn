#include "Packer.h"
#include "AssetNormalize.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <zstd.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Assets/PackFormat.h"
#include "Assets/PackMetadataJson.h"

#include "FileIO.h"

using namespace Blackthorn::Assets;

namespace BTPacker {

namespace {

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

	if (!Blackthorn::seekTo(f, 0, SEEK_END)) {
		std::fclose(f);
		return false;
	}

	const int64_t size = Blackthorn::fileTell(f);

	if (!Blackthorn::seekTo(f, 0, SEEK_SET)) {
		std::fclose(f);
		return false;
	}

	if (size < 0) {
		std::fclose(f);
		std::cerr << "btpacker: error: cannot determine size of '" << path.string() << "'\n";
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const bool ok = Blackthorn::readExact(f, out.data(), out.size());
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
	if (lower == "spriteclip")
		return PackAssetType::SpriteClip;
	if (lower == "particleeffect")
		return PackAssetType::ParticleEffect;
	if (lower == "localization")
		return PackAssetType::Localization;
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
		case PackAssetType::SpriteClip:
			return "SpriteClip";
		case PackAssetType::ParticleEffect:
			return "ParticleEffect";
		case PackAssetType::Localization:
			return "Localization";
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
	if (!Blackthorn::seekTo(f, 0)) {
		std::cerr << "btpacker: error: seek failed in '" << filePath << "'\n";
		return false;
	}

	if (!Blackthorn::readExact(f, &header, sizeof(BTPHeader))) {
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
	if (!Blackthorn::seekTo(f, header.tocOffset)) {
		std::cerr << "btpacker: error: seek to TOC failed in '" << filePath << "'\n";
		return false;
	}

	std::vector<uint8_t> compressedTOC(static_cast<size_t>(header.tocCompSize));
	if (!Blackthorn::readExact(f, compressedTOC.data(), compressedTOC.size())) {
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
	std::unordered_map<uint64_t, std::string>& sources
) {
	if (header.symbolTableOff == 0 || header.symbolTableSize == 0)
		return;

	if (!Blackthorn::seekTo(f, header.symbolTableOff))
		return;

	std::vector<uint8_t> block(static_cast<size_t>(header.symbolTableSize));
	if (!Blackthorn::readExact(f, block.data(), block.size()))
		return;

	const uint8_t* cursor = block.data();
	const uint8_t* end = block.data() + block.size();

	while (cursor < end) {
		if (cursor + sizeof(uint64_t) > end)
			break;

		uint64_t assetID = 0;
		std::memcpy(&assetID, cursor, sizeof(uint64_t));
		cursor += sizeof(uint64_t);

		const uint8_t* pathStart = cursor;
		while (cursor < end && *cursor != '\0')
			++cursor;

		std::string srcPath(reinterpret_cast<const char*>(pathStart),
							static_cast<size_t>(cursor - pathStart));

		if (cursor < end)
			++cursor;

		sources[assetID] = std::move(srcPath);
	}
}

/**
 * @brief Appends one entry to the in-memory symbol table byte buffer.
 *
 * Binary layout per entry:
 *   uint64_t  assetID
 *   char      assetPath[]     (null-terminated canonical ID, e.g. "assets/textures/player.png")
 *
 * The reader in readSymbolTable() expects exactly this layout.
 *
 * @param buf         Buffer to append to.
 * @param assetID     xxHash64 of the canonical ID.
 * @param path        The canonical ID. Same string the engine passes to load().
 */
void appendSymbolEntry(
	std::vector<uint8_t>& buf,
	uint64_t assetID,
	const std::string& path
) {
	// -- assetID (8 bytes) --
	const size_t idStart = buf.size();
	buf.resize(idStart + sizeof(uint64_t));
	std::memcpy(buf.data() + idStart, &assetID, sizeof(uint64_t));

	// -- relative source path (null-terminated) --
	const size_t pathStart = buf.size();
	buf.resize(pathStart + path.size() + 1);
	std::memcpy(buf.data() + pathStart, path.data(), path.size());
	buf[pathStart + path.size()] = '\0';
}

std::optional<PackMetadata> loadPackMetadata(
	const std::filesystem::path& metadataPath,
	std::ostream& log
) {
	if (metadataPath.empty())
		return std::nullopt;

	std::ifstream file(metadataPath, std::ios::in);
	if (!file.is_open()) {
		std::cerr << "btpacker: warning: cannot open '" << metadataPath.string() << "', skipping pack metadata\n";
		return std::nullopt;
	}

	std::ostringstream ss;
	ss << file.rdbuf();

	const nlohmann::json json = nlohmann::json::parse(ss.str(), nullptr, false);

	if (json.is_discarded() || !json.is_object()) {
		std::cerr << "btpacker: warning: '" << metadataPath.string() << "' is not valid JSON, skipping pack metadata\n";
		return std::nullopt;
	}

	const PackMetadata metadata = parsePackMetadata(json, [&metadataPath](const std::string& warning) {
		std::cerr << "btpacker: warning: '" << metadataPath.string() << "': " << warning << "\n";
	});

	if (metadata.empty()) {
		std::cerr << "btpacker: warning: '" << metadataPath.string() << "' has no usable fields, skipping pack metadata\n";
		return std::nullopt;
	}

	log << "  metadata   " << metadataPath.string() << "\n\n";
	return metadata;
}

/**
 * @brief Reads and parses the pack metadata block from an open file.
 * No-op (returns std::nullopt) if header.metadataOff is 0, the block can't
 * be read, or it isn't valid JSON. Readers should treat a corrupt
 * metadata block as "absent", not as a reason to fail the whole operation.
 */
std::optional<PackMetadata> readPackMetadata(std::FILE* f, const BTPHeader& header) {
	if (header.metadataOff == 0 || header.metadataSize == 0)
		return std::nullopt;

	if (!Blackthorn::seekTo(f, header.metadataOff))
		return std::nullopt;

	std::string text(static_cast<size_t>(header.metadataSize), '\0');
	if (!Blackthorn::readExact(f, text.data(), text.size()))
		return std::nullopt;

	const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded() || !json.is_object())
		return std::nullopt;

	return parsePackMetadata(json);
}

/**
 * @brief Reads the metadata block exactly as embedded; these are the bytes
 * the pack digest covers.
 *
 * @return The bytes (empty if the pack has no metadata block), or
 *         std::nullopt if a block is declared but cannot be read.
 */
std::optional<std::string> readMetadataBytes(std::FILE* f, const BTPHeader& header) {
	if (header.metadataOff == 0 || header.metadataSize == 0)
		return std::string();

	if (!Blackthorn::seekTo(f, header.metadataOff))
		return std::nullopt;

	std::string text(static_cast<size_t>(header.metadataSize), '\0');
	if (!Blackthorn::readExact(f, text.data(), text.size()))
		return std::nullopt;

	return text;
}

} // anonymous namespace

bool Packer::pack(const PackManifest& manifest, const PackOptions& opts, std::ostream& log) {
	const std::optional<PackMetadata> packMetadata = manifest.metadataPath.empty()
		? std::nullopt
		: loadPackMetadata(opts.input / manifest.metadataPath, log);
	const auto outDir = opts.output.parent_path();

	if (!outDir.empty() && !std::filesystem::exists(outDir)) {
		std::error_code ec;
		std::filesystem::create_directories(outDir, ec);

		if (ec) {
			std::cerr << "btpacker: error: cannot create output directory '"
					  << outDir.string() << "': " << ec.message() << "\n";

			return false;
		}
	}

	const std::string outPathStr = opts.output.string();
	std::FILE* out = std::fopen(outPathStr.c_str(), "wb");

	if (!out) {
		std::cerr << "btpacker: error: cannot create output file '" << outPathStr << "'\n";
		return false;
	}

	BTPHeader header{};
	if (!Blackthorn::writeExact(out, &header, sizeof(BTPHeader))) {
		std::cerr << "btpacker: error: cannot write header placeholder\n";
		std::fclose(out);
		std::filesystem::remove(opts.output);
		return false;
	}

	std::vector<BTPEntry> toc;
	toc.reserve(manifest.assets.size());

	std::vector<uint8_t> symbolTableBytes;

	uint64_t totalSourceBytes = 0;
	uint64_t totalCompressedBytes = 0;

	std::unordered_map<uint64_t, std::string> seenIDs;

	for (const ManifestAsset& asset : manifest.assets) {
		const auto normalized = Blackthorn::normalizeAssetPath(asset.sourcePath.generic_string());
		if (!normalized) {
			std::cerr << "btpacker: error: invalid asset path '" << asset.sourcePath.generic_string() << "'\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}

		const std::string forHash = manifest.virtualRoot + "/" + *normalized;

		const uint64_t assetID = XXH64(forHash.data(), forHash.size(), 0);

		if (auto [it, inserted] = seenIDs.emplace(assetID, *normalized); !inserted) {
			std::cerr << "btpacker: error: '" << *normalized << "' collides with '"
					  << it->second << "' (hash 0x" << std::hex << assetID << std::dec << ")\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}

		const std::filesystem::path assetSource = opts.input / *normalized;

		std::vector<uint8_t> raw;
		if (!readFile(assetSource, raw)) {
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}

		std::vector<uint8_t> compressed;
		if (!compressZstd(raw, compressed, manifest.compressionLevel)) {
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}

		const uint64_t blobHash = XXH64(compressed.data(), compressed.size(), 0);

		const int64_t dataOffset = Blackthorn::fileTell(out);
		if (dataOffset < 0) {
			std::cerr << "btpacker: error: ftell failed while writing '" << assetSource << "'\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}

		if (!Blackthorn::writeExact(out, compressed.data(), compressed.size())) {
			std::cerr << "btpacker: error: write failed for asset '" << assetSource << "'\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
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
				forHash
			);
		}

		const float ratio = raw.empty() ? 0.0f
			: (1.0f - static_cast<float>(compressed.size())
				/ static_cast<float>(raw.size())) * 100.0f;

		log << "  packed  " << *normalized
			<< "  [" << assetTypeName(entry.assetType) << "]"
			<< "  " << raw.size() << " B  ->  " << compressed.size() << " B"
			<< "  (" << std::fixed << std::setprecision(1) << ratio << "% smaller)\n";

		totalSourceBytes += raw.size();
		totalCompressedBytes += compressed.size();
	}

	const uint64_t tocOffset = static_cast<uint64_t>(Blackthorn::fileTell(out));

	const size_t tocRawSize = toc.size() * sizeof(BTPEntry);
	std::vector<uint8_t> tocRaw(tocRawSize);
	std::memcpy(tocRaw.data(), toc.data(), tocRawSize);

	std::vector<uint8_t> tocCompressed;
	if (!compressZstd(tocRaw, tocCompressed, manifest.compressionLevel)) {
		std::fclose(out);
		std::filesystem::remove(opts.output);
		return false;
	}

	if (!Blackthorn::writeExact(out, tocCompressed.data(), tocCompressed.size())) {
		std::cerr << "btpacker: error: write failed for TOC\n";
		std::fclose(out);
		std::filesystem::remove(opts.output);
		return false;
	}

	uint64_t symbolTableOff = 0;
	uint64_t symbolTableSize = 0;

	if (manifest.writeSymbolTable && !symbolTableBytes.empty()) {
		symbolTableOff = static_cast<uint64_t>(Blackthorn::fileTell(out));
		symbolTableSize = static_cast<uint64_t>(symbolTableBytes.size());

		if (!Blackthorn::writeExact(out, symbolTableBytes.data(), symbolTableBytes.size())) {
			std::cerr << "btpacker: error: write failed for symbol table\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
			return false;
		}
	}

	std::string metaText;
	uint64_t metadataOff = 0;
	uint64_t metadataSize = 0;

	if (packMetadata) {
		metaText = serializePackMetadata(*packMetadata);

		metadataOff = static_cast<uint64_t>(Blackthorn::fileTell(out));
		metadataSize = static_cast<uint64_t>(metaText.size());

		if (!Blackthorn::writeExact(out, metaText.data(), metaText.size())) {
			std::cerr << "btpacker: error: write failed for pack metadata\n";
			std::fclose(out);
			std::filesystem::remove(opts.output);
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
	header.metadataOff = metadataOff;
	header.metadataSize = metadataSize;

	if (!Blackthorn::seekTo(out, 0) || !Blackthorn::writeExact(out, &header, sizeof(BTPHeader))) {
		std::cerr << "btpacker: error: failed to patch header\n";
		std::fclose(out);
		std::filesystem::remove(opts.output);
		return false;
	}

	std::fclose(out);

	const float overallRatio = (totalSourceBytes == 0) ? 0.0f
		: (1.0f - static_cast<float>(totalCompressedBytes)
			/ static_cast<float>(totalSourceBytes)) * 100.0f;

	const auto fileSize = std::filesystem::file_size(opts.output);

	log << "\n"
		<< "  output:       " << outPathStr << "\n"
		<< "  assets:       " << toc.size() << "\n"
		<< "  source size:  " << totalSourceBytes << " B\n"
		<< "  pack size:    " << fileSize << " B\n"
		<< "  reduction:    " << std::fixed << std::setprecision(1)
		<< overallRatio << "%\n"
		<< "  symbol table: " << (manifest.writeSymbolTable ? "yes" : "no")     << "\n"
		<< "  metadata:     " << (packMetadata ? "yes" : "no")                 << "\n";

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

	std::unordered_map<uint64_t, std::string> sources;
	readSymbolTable(f, header, sources);

	log << "verifying '" << pathStr << "' (" << entries.size() << " entries)...\n";

	int passed = 0;
	int failed = 0;

	for (const BTPEntry& entry : entries) {
		if (!Blackthorn::seekTo(f, entry.dataOffset)) {
			std::cerr << "  FAIL  0x" << std::hex << entry.assetID
					  << "; seek error\n" << std::dec;
			++failed;
			continue;
		}

		std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressedSize));
		if (!Blackthorn::readExact(f, compressed.data(), compressed.size())) {
			std::cerr << "  FAIL  0x" << std::hex << entry.assetID
					  << "; read error\n" << std::dec;

			++failed;
			continue;
		}

		const uint64_t actualHash = XXH64(compressed.data(), compressed.size(), 0);
		if (actualHash != entry.xxhash) {
			std::cerr << "  FAIL  ";
			auto it = sources.find(entry.assetID);

			if (it != sources.end()) {
				std::cerr << it->second;
			} else {
				std::cerr << "0x" << std::hex << entry.assetID << std::dec;
			}

			std::cerr << "; hash mismatch (expected 0x" << std::hex
					  << entry.xxhash << ", got 0x" << actualHash << ")\n" << std::dec;

			++failed;
			continue;
		}

		if (entry.compression == PackCompression::Zstd) {
			std::vector<uint8_t> raw;
			if (!decompressZstd(compressed, raw, entry.uncompressedSize)) {
				std::cerr << "  FAIL  0x" << std::hex << entry.assetID
						  << "; decompression error\n" << std::dec;

				++failed;
				continue;
			}
		}

		auto it = sources.find(entry.assetID);
		const std::string label = (it != sources.end())
			? it->second
			: [&]{ std::ostringstream ss; ss << "0x" << std::hex << entry.assetID; return ss.str(); }();

		log << "  ok    " << label << "\n";
		++passed;
	}

	bool integrityOk = true;

	const std::optional<std::string> metadataBytes = readMetadataBytes(f, header);
	if (!metadataBytes) {
		std::cerr << "  FAIL  pack metadata block cannot be read\n";
		integrityOk = false;
	}

	std::fclose(f);

	log << "\n  passed: " << passed << " / " << (passed + failed) << "\n";

	return failed == 0 && integrityOk;
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

	std::unordered_map<uint64_t, std::string> sources;
	readSymbolTable(f, header, sources);

	const std::optional<PackMetadata> packMetadata = readPackMetadata(f, header);
	const std::optional<std::string> metadataBytes = readMetadataBytes(f, header);

	std::fclose(f);

	const auto fileSize = std::filesystem::file_size(btpPath);

	log << "pack: " << pathStr << "\n"
		<< "  version:      " << header.version << "\n"
		<< "  entries:      " << header.entryCount << "\n"
		<< "  file size:    " << fileSize << " B\n"
		<< "  symbol table: " << (header.symbolTableOff != 0 ? "yes" : "no") << "\n"
		<< "\n";

	if (packMetadata) {
		log << "  metadata:\n";

		if (!packMetadata->id.empty())
			log << "    id:                 " << packMetadata->id << "\n";

		if (!packMetadata->name.empty())
			log << "    name:               " << packMetadata->name << "\n";

		if (!packMetadata->version.empty())
			log << "    version:            " << packMetadata->version << "\n";

		if (!packMetadata->author.empty())
			log << "    author:             " << packMetadata->author << "\n";

		if (!packMetadata->shortDescription.empty())
			log << "    short description:  " << packMetadata->shortDescription << "\n";

		if (!packMetadata->longDescription.empty())
			log << "    long description:   " << packMetadata->longDescription << "\n";

		if (!packMetadata->dependencies.empty()) {
			log << "    dependencies:       ";

			for (size_t i = 0; i < packMetadata->dependencies.size(); ++i) {
				const PackDependency& dep = packMetadata->dependencies[i];
				log << (i ? ", " : "") << dep.id;

				if (!dep.versionRange.empty())
					log << " " << dep.versionRange;
			}

			log << "\n";
		}

		const auto printIDs = [&log](const char* label, const std::vector<std::string>& ids) {
			if (ids.empty())
				return;

			log << "    " << label;
			for (size_t i = 0; i < ids.size(); ++i)
				log << (i ? ", " : "") << ids[i];

			log << "\n";
		};

		printIDs("load before:        ", packMetadata->loadBefore);
		printIDs("load after:         ", packMetadata->loadAfter);
		printIDs("conflicts:          ", packMetadata->conflicts);
	} else {
		log << "  metadata:     no\n";
	}

	log << "\n";

	log << std::left
		<< std::setw(20) << "asset ID (hex)"
		<< std::setw(16) << "type"
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
			<< std::setw(16) << assetTypeName(entry.assetType)
			<< std::setw(8)  << compressionName(entry.compression)
			<< std::setw(14) << entry.uncompressedSize
			<< std::setw(14) << entry.compressedSize;

		auto srcIt = sources.find(entry.assetID);

		if (srcIt != sources.end() && !srcIt->second.empty())
			log << srcIt->second;

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

	std::unordered_map<uint64_t, std::string> sources;
	readSymbolTable(f, header, sources);

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
		if (!Blackthorn::seekTo(f, entry.dataOffset)) {
			std::cerr << "btpacker: error: seek failed for entry 0x"
					  << std::hex << entry.assetID << std::dec << "\n";
			anyFailed = true;
			continue;
		}

		std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressedSize));
		if (!Blackthorn::readExact(f, compressed.data(), compressed.size())) {
			std::cerr << "btpacker: error: read failed for entry 0x"
					  << std::hex << entry.assetID << std::dec << "\n";

			anyFailed = true;
			continue;
		}

		std::filesystem::path outFile;
		auto srcIt = sources.find(entry.assetID);
		const auto safeName = (srcIt != sources.end()) ? Blackthorn::normalizeAssetPath(srcIt->second) : std::nullopt;
		if (safeName) {
			outFile = destDir / *safeName;
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

		const bool wrote = Blackthorn::writeExact(outF, raw.data(), raw.size());
		std::fclose(outF);

		if (!wrote) {
			std::cerr << "btpacker: error: write failed for '" << outFile.string() << "'\n";
			anyFailed = true;
			continue;
		}

		const std::string label = (srcIt != sources.end())
			? srcIt->second
			: [&]{ std::ostringstream s; s << "0x" << std::hex << entry.assetID; return s.str(); }();

		log << "  unpacked  " << label << "  ->  " << outFile.string() << "\n";
	}

	std::fclose(f);
	return !anyFailed;
}

} // namespace BTPacker