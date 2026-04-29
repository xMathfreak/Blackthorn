#include "Saves/Storage/LocalFileSaveStorage.h"

#include <fstream>

#include "Debug/Logger.h"
#include "Saves/SaveDocument.h"

namespace Blackthorn::Saves {

LocalFileSaveStorage::LocalFileSaveStorage(
	std::filesystem::path root,
	PathResolver res
)
	: rootDir(std::move(root))
	, resolver(std::move(res))
{}

void LocalFileSaveStorage::setPathResolver(PathResolver r) {
	resolver = std::move(r);
}

std::filesystem::path LocalFileSaveStorage::resolvePath(const SaveId& saveId) const {
	if (resolver)
		return resolver(saveId);
	return defaultPath(saveId);
}

std::filesystem::path LocalFileSaveStorage::defaultPath(const SaveId& saveId) const {
	std::filesystem::path path = rootDir;

	if (!saveId.worldId.empty())
		path /= saveId.worldId;

	if (!saveId.playerId.empty())
		path /= saveId.playerId;

	path /= (saveId.id.toString() + ".sav");
	return path;
}

SaveResult LocalFileSaveStorage::write(
	const SaveId& saveId,
	const IO::ByteBuffer& data
) {
	const std::filesystem::path path = resolvePath(saveId);

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

	if (ec) {
		return SaveResult::failure(
			"Failed to create save directory '" +
			path.parent_path().string() + "': " + ec.message()
		);
	}

	std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file.is_open())
		return SaveResult::failure("Failed to open '" + path.string() + "' for writing");

	file.write(
		reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size())
	);

	if (!file)
		return SaveResult::failure("Write error on '" + path.string() + "'");

	BT_DEBUG(
		"LocalFileSaveStorage: wrote {} bytes to '{}'",
		data.size(), path.string()
	);

	return SaveResult::success();
}

SaveReadResult LocalFileSaveStorage::read(const SaveId& saveId) {
	const std::filesystem::path path = resolvePath(saveId);

	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		return SaveReadResult::failure("Failed to open '" + path.string() + "' for reading");
	}

	file.seekg(0, std::ios::end);
	const auto fileSize = static_cast<size_t>(file.tellg());
	file.seekg(0, std::ios::beg);

	if (fileSize == 0) {
		return SaveReadResult::failure("Save file '" + path.string() + "' is empty");
	}

	std::vector<U8> buf(fileSize);
	file.read(reinterpret_cast<char*>(buf.data()),
		static_cast<std::streamsize>(fileSize));

	if (!file) {
		return SaveReadResult::failure("Read error on '" + path.string() + "'");
	}

	BT_DEBUG(
		"LocalFileSaveStorage: read {} bytes from '{}'",
		fileSize, path.string()
	);

	return SaveReadResult::success(
		IO::ByteBuffer(std::move(buf))
	);
}

SaveResult LocalFileSaveStorage::remove(const SaveId& saveId) {
	const std::filesystem::path path = resolvePath(saveId);

	std::error_code ec;
	std::filesystem::remove(path, ec);

	if (ec) {
		return SaveResult::failure(
			"Failed to remove '" + path.string() + "': " + ec.message());
	}

	return SaveResult::success();
}

bool LocalFileSaveStorage::exists(const SaveId& saveId) {
	return std::filesystem::exists(resolvePath(saveId));
}

std::vector<SaveMetadata> LocalFileSaveStorage::list(const SaveFilter& filter) {
	std::vector<SaveMetadata> results;

	std::error_code ec;
	if (!std::filesystem::exists(rootDir, ec))
		return results;

	for (const auto& entry :
		std::filesystem::recursive_directory_iterator(rootDir, ec))
	{
		if (ec) {
			BT_WARN("LocalFileSaveStorage: directory iteration error — {}",
				ec.message());
			break;
		}

		if (!entry.is_regular_file())
			continue;

		if (entry.path().extension() != ".sav")
			continue;

		std::ifstream file(entry.path(), std::ios::in | std::ios::binary);
		if (!file.is_open())
			continue;

		file.seekg(0, std::ios::end);
		const auto fileSize = static_cast<size_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		if (fileSize < FileHeader::SERIALIZED_SIZE)
			continue;

		std::vector<U8> buf(fileSize);
		file.read(reinterpret_cast<char*>(buf.data()),
			static_cast<std::streamsize>(fileSize));

		if (!file)
			continue;

		IO::ByteBuffer rawBuf(std::move(buf));

		SaveDocument doc;
		if (!doc.parse(rawBuf)) {
			BT_WARN(
				"LocalFileSaveStorage: skipping invalid save file '{}'",
				entry.path().string()
			);

			continue;
		}

		SaveId sid;
		sid.createdAt = doc.getHeader().createdAt;
		sid.updatedAt = doc.getHeader().updatedAt;

		const std::string stem = entry.path().stem().string();
		sid.id = UUID::fromString(stem);

		if (sid.id.isNull()) {
			BT_DEBUG(
				"LocalFileSaveStorage: save '{}' has non-UUID filename",
				entry.path().string()
			);
		}

		if (!filter.matches(sid))
			continue;

		SaveMetadata meta;
		meta.saveId = sid;
		meta.formatVersion = doc.getHeader().formatVersion;

		for (const auto& tableEntry : doc.getSectionTable())
			meta.sectionIds.push_back(tableEntry.sectionId);

		results.push_back(std::move(meta));
	}

	return results;
}

} // namespace Blackthorn::Saves