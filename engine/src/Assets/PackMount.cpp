#include "Assets/PackMount.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <nlohmann/json.hpp>
#include <zstd.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Assets/PackMetadataJson.h"
#include "Debug/Logger.h"
#include "FileIO.h"

namespace Blackthorn::Assets {

bool PackMount::mount(const std::filesystem::path& path, U32 priority) {
	packPath = path;
	packPriority = priority;
	mounted = false;
	contentMap.clear();

#ifdef BLACKTHORN_DEBUG
	symbols.clear();
	sources.clear();
#endif

	std::FILE* file = std::fopen(path.string().c_str(), "rb");
	if (!file) {
		BT_ERROR("PackMount: failed to open '{}'", path.string());
		return false;
	}

	BTPHeader header{};
	if (!readExact(file, &header, sizeof(BTPHeader))) {
		BT_ERROR("PackMount: '{}': could not read header", path.string());
		std::fclose(file);
		return false;
	}

	if (header.magic != BTP_MAGIC) {
		BT_ERROR(
			"PackMount: '{}': bad magic 0x{:08X} (expected 0x{:08X})",
			path.string(), header.magic, BTP_MAGIC
		);
		std::fclose(file);
		return false;
	}

	if (header.version != BTP_VERSION) {
		BT_ERROR(
			"PackMount: '{}': unsupported version {} (expected {})",
			path.string(), header.version, BTP_VERSION
		);
		std::fclose(file);
		return false;
	}

	if (header.entryCount == 0)
		BT_WARN("PackMount: '{}': pack is empty (entryCount == 0)", path.string());

	if (!loadTOC(file, header)) {
		std::fclose(file);
		return false;
	}

#ifdef BLACKTHORN_DEBUG
	if (header.symbolTableOff != 0 && header.symbolTableSize != 0)
		loadSymbolTable(file, header);
#endif

	const std::string metadataBytes = readMetadataBytes(file, header);
	loadMetadata(metadataBytes);

	std::fclose(file);
	mounted = true;

	BT_LOG(
		"PackMount: '{}' mounted: {} asset(s), priority {}",
		path.string(), contentMap.size(), packPriority
	);

	return true;
}

bool PackMount::loadTOC(std::FILE* file, const BTPHeader& header) {
	if (!seekTo(file, header.tocOffset)) {
		BT_ERROR("PackMount: '{}': seek to TOC offset {} failed", packPath.string(), header.tocOffset);
		return false;
	}

	std::vector<U8> compressedTOC(static_cast<size_t>(header.tocCompSize));
	if (!readExact(file, compressedTOC.data(), compressedTOC.size())) {
		BT_ERROR("PackMount: '{}': could not read {} TOC bytes", packPath.string(), header.tocCompSize);
		return false;
	}

	std::vector<U8> rawTOC(static_cast<size_t>(header.tocUncompSize));
	const size_t result = ZSTD_decompress(
		rawTOC.data(), rawTOC.size(),
		compressedTOC.data(), compressedTOC.size()
	);

	if (ZSTD_isError(result)) {
		BT_ERROR(
			"PackMount: '{}': TOC decompression failed: {}",
			packPath.string(), ZSTD_getErrorName(result)
		);
		return false;
	}

	if (result != rawTOC.size()) {
		BT_ERROR(
			"PackMount: '{}': TOC decompressed size mismatch: expected {}, got {}",
			packPath.string(), rawTOC.size(), result
		);
		return false;
	}

	if (result % sizeof(BTPEntry) != 0) {
		BT_ERROR(
			"PackMount: '{}': decompressed TOC size {} is not a multiple of BTPEntry ({})",
			packPath.string(), result, sizeof(BTPEntry)
		);
		return false;
	}

	const size_t count = result / sizeof(BTPEntry);
	if (count != static_cast<size_t>(header.entryCount)) {
		BT_WARN(
			"PackMount: '{}': TOC entry count mismatch: header says {}, TOC has {}",
			packPath.string(), header.entryCount, count
		);
	}

	contentMap.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		BTPEntry entry{};
		std::memcpy(
			&entry,
			rawTOC.data() + i * sizeof(BTPEntry),
			sizeof(BTPEntry)
		);

		if (!contentMap.emplace(entry.assetID, entry).second) {
			BT_ERROR(
				"PackMount: '{}': duplicate asset ID 0x{:016X} in TOC",
				packPath.string(), entry.assetID
			);
			return false;
		}
	}

	return true;
}

#ifdef BLACKTHORN_DEBUG
void PackMount::loadSymbolTable(std::FILE* file, const BTPHeader& header) {
	if (!seekTo(file, header.symbolTableOff)) {
		BT_WARN("PackMount: '{}': seek to symbol table failed", packPath.string());
		return;
	}

	std::vector<U8> block(static_cast<size_t>(header.symbolTableSize));
	if (!readExact(file, block.data(), block.size())) {
		BT_WARN("PackMount: '{}': could not read symbol table", packPath.string());
		return;
	}

	const U8* cursor = block.data();
	const U8* end = block.data() + block.size();

	while (cursor < end) {
		if (cursor + sizeof(U64) > end)
			break;

		U64 assetID = 0;
		std::memcpy(&assetID, cursor, sizeof(U64));
		cursor += sizeof(U64);

		const U8* pathStart = cursor;
		while (cursor < end && *cursor != '\0')
			++cursor;

		std::string srcPath(reinterpret_cast<const char*>(pathStart),
		 static_cast<size_t>(cursor - pathStart));

		if (cursor < end)
			++cursor;

		sources.emplace(assetID, std::move(srcPath));
	}

	BT_DEBUG("PackMount: '{}': loaded {} symbol(s)", packPath.string(), sources.size());
}
#endif

bool PackMount::has(U64 assetID) const {
	return contentMap.contains(assetID);
}

std::optional<PackedAssetData> PackMount::read(U64 assetID) const {
	auto it = contentMap.find(assetID);
	if (it == contentMap.end())
		return std::nullopt;

	const BTPEntry& entry = it->second;

	std::FILE* file = std::fopen(packPath.string().c_str(), "rb");
	if (!file) {
		BT_ERROR("PackMount: failed to open '{}'", packPath.string());
		return std::nullopt;
	}

	if (!seekTo(file, entry.dataOffset)) {
		BT_ERROR(
			"PackMount: seek to offset {} failed in '{}'",
			entry.dataOffset, packPath.string()
		);
		std::fclose(file);
		return std::nullopt;
	}

	std::vector<U8> compressed(static_cast<size_t>(entry.compressedSize));
	if (!readExact(file, compressed.data(), compressed.size())) {
		BT_ERROR(
			"PackMount: could not read {} compressed bytes for asset 0x{:016X} in '{}'",
			entry.compressedSize, assetID, packPath.string()
		);
		std::fclose(file);
		return std::nullopt;
	}

	std::fclose(file);

	const U64 actualHash = XXH64(
		compressed.data(),
		compressed.size(),
		0
	);

	if (actualHash != entry.xxhash) {
		BT_ERROR(
			"PackMount: hash mismatch for asset 0x{:016X} in '{}' "
			"(expected 0x{:016X}, got 0x{:016X}). File may be corrupt.",
			assetID, packPath.string(), entry.xxhash, actualHash
		);
		return std::nullopt;
	}

	PackedAssetData result;
	result.assetType = entry.assetType;
	result.bytes.resize(static_cast<size_t>(entry.uncompressedSize));

	if (entry.compression == PackCompression::Zstd) {
		const size_t decompResult = ZSTD_decompress(
			result.bytes.data(), result.bytes.size(),
			compressed.data(), compressed.size()
		);

		if (ZSTD_isError(decompResult)) {
			BT_ERROR(
				"PackMount: decompression failed for asset 0x{:016X} in '{}': {}",
				assetID, packPath.string(), ZSTD_getErrorName(decompResult)
			);
			return std::nullopt;
		}

		if (decompResult != result.bytes.size()) {
			BT_ERROR(
				"PackMount: decompressed size mismatch for asset 0x{:016X} in '{}': "
				"expected {}, got {}",
				assetID, packPath.string(), result.bytes.size(), decompResult
			);
			return std::nullopt;
		}
	} else {
		if (compressed.size() != result.bytes.size()) {
			BT_ERROR(
				"PackMount: uncompressed asset 0x{:016X} in '{}' has size mismatch: "
				"expected {}, got {}",
				assetID, packPath.string(), result.bytes.size(), compressed.size()
			);
			return std::nullopt;
		}

		std::memcpy(result.bytes.data(), compressed.data(), compressed.size());
	}

#ifdef BLACKTHORN_DEBUG
	auto srcIt = sources.find(assetID);
	if (srcIt != sources.end())
		result.sourcePath = srcIt->second;
#endif

	return result;
}

void PackMount::loadMetadata(const std::string& metadataBytes) {
	if (metadataBytes.empty())
		return;

	const nlohmann::json json = nlohmann::json::parse(metadataBytes, nullptr, false);
	if (json.is_discarded() || !json.is_object()) {
		BT_WARN("PackMount: '{}': metadata block is not valid JSON, ignoring", packPath.string());
		return;
	}

	metadata = parsePackMetadata(json, [this](const std::string& warning) {
		BT_WARN("PackMount: '{}': metadata: {}", packPath.string(), warning);
	});

	BT_DEBUG(
		"PackMount: '{}': loaded metadata (id='{}', name='{}', version='{}')",
		packPath.string(), metadata.id, metadata.name, metadata.version
	);
}

std::string PackMount::readMetadataBytes(std::FILE* file, const BTPHeader& header) {
	if (header.metadataOff == 0 || header.metadataSize == 0)
		return {};

	if (!seekTo(file, header.metadataOff)) {
		BT_WARN("PackMount: '{}': seek to metadata block failed", packPath.string());
		return {};
	}

	std::string text(static_cast<size_t>(header.metadataSize), '\0');
	if (!readExact(file, text.data(), text.size())) {
		BT_WARN("PackMount: '{}': could not read metadata block", packPath.string());
		return {};
	}

	return text;
}


} // namespace Blackthorn::Assets
