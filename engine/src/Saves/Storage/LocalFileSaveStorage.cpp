#include "Saves/Storage/LocalFileSaveStorage.h"

#include <cstdio>
#include <fstream>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
#endif

#include "Debug/Logger.h"
#include "Saves/SaveDocument.h"

namespace Blackthorn::Saves {

LocalFileSaveStorage::LocalFileSaveStorage(
	std::filesystem::path root,
	const std::string& ext,
	PathResolver res
)
	: rootDir(std::move(root))
	, extension(ext)
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

	path /= (saveId.id.toString() + extension);
	return path;
}

SaveResult LocalFileSaveStorage::write(
	const SaveId& saveId,
	const IO::ByteBuffer& data
) {
	const std::filesystem::path path = resolvePath(saveId);
	const auto dir = path.parent_path().empty()
		? std::filesystem::current_path()
		: path.parent_path();

	const UMAX needed = static_cast<UMAX>(data.size());
	const UMAX available = availableBytes(dir);

	if (available < needed) {
		return SaveResult::failure(
			"Insufficient disk space: need "
			+ std::to_string(needed)
			+ " bytes, only "
			+ std::to_string(available)
			+ " available on '"
			+ path.parent_path().string() + "'"
		);
	}

	BT_DEBUG(
		"LocalFileSaveStorage: writing {} bytes to '{}' ({} bytes available)",
		data.size(), path.string(), available
	);

	return atomicWrite(path, data);
}

SaveReadResult LocalFileSaveStorage::read(const SaveId& saveId) {
	const std::filesystem::path path = resolvePath(saveId);

	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		return SaveReadResult::failure(
			"Failed to open '" + path.string() + "' for reading"
		);
	}

	file.seekg(0, std::ios::end);
	const auto fileSize = static_cast<size_t>(file.tellg());
	file.seekg(0, std::ios::beg);

	if (fileSize == 0)
		return SaveReadResult::failure("Save file '" + path.string() + "' is empty");

	std::vector<U8> buf(fileSize);
	file.read(reinterpret_cast<char*>(buf.data()),
		static_cast<std::streamsize>(fileSize));

	if (!file)
		return SaveReadResult::failure("Read error on '" + path.string() + "'");

	BT_DEBUG(
		"LocalFileSaveStorage: read {} bytes from '{}'",
		fileSize, path.string()
	);

	return SaveReadResult::success(IO::ByteBuffer(std::move(buf)));
}

SaveResult LocalFileSaveStorage::remove(const SaveId& saveId) {
	const std::filesystem::path path = resolvePath(saveId);

	std::error_code ec;
	std::filesystem::remove(path, ec);

	if (ec) {
		return SaveResult::failure(
			"Failed to remove '" + path.string() + "': " + ec.message()
		);
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

		if (entry.path().extension() != extension)
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

		doc.getSaveIdBlock().applyToSaveId(sid);

		const std::string stem = entry.path().stem().string();
		sid.id = Core::UUID::fromString(stem);

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

UMAX LocalFileSaveStorage::availableBytes(
	const std::filesystem::path& path
) {
	std::filesystem::path probe = path;

	std::error_code ec;
	while (!probe.empty() && !std::filesystem::exists(probe, ec))
		probe = probe.parent_path();

	if (probe.empty())
		probe = std::filesystem::current_path(ec);

	if (ec)
		return 0;

	const auto si = std::filesystem::space(probe, ec);

	if (ec)
		return 0;

	return si.available;
}

SaveResult LocalFileSaveStorage::atomicWrite(
	const std::filesystem::path& destination,
	const IO::ByteBuffer& data
) {
	const std::filesystem::path dir = destination.parent_path();

#ifdef _WIN32
	const auto pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
	const auto pid = static_cast<unsigned long>(::getpid());
#endif

	const std::filesystem::path tempPath =
		dir / (destination.stem().string()
			+ ".tmp."
			+ std::to_string(pid)
			+ destination.extension().string());

	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		return SaveResult::failure(
			"atomicWrite: failed to create directory '"
			+ dir.string() + "': " + ec.message()
		);
	}

#ifdef _WIN32
	HANDLE hFile = ::CreateFileW(
		tempPath.wstring().c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hFile == INVALID_HANDLE_VALUE) {
		return SaveResult::failure(
			"atomicWrite: failed to open temp file '"
			+ tempPath.string() + "' for writing (Win32 error "
			+ std::to_string(::GetLastError()) + ")"
		);
	}

	DWORD written = 0;
	const BOOL writeOk = ::WriteFile(
		hFile,
		data.data(),
		static_cast<DWORD>(data.size()),
		&written,
		nullptr
	);

	if (!writeOk || written != static_cast<DWORD>(data.size())) {
		const DWORD err = ::GetLastError();
		::CloseHandle(hFile);
		std::filesystem::remove(tempPath, ec);
		return SaveResult::failure(
			"atomicWrite: write to temp file failed (Win32 error "
			+ std::to_string(err) + ")"
		);
	}

	if (!::FlushFileBuffers(hFile)) {
		const DWORD err = ::GetLastError();
		::CloseHandle(hFile);
		std::filesystem::remove(tempPath, ec);
		return SaveResult::failure(
			"atomicWrite: FlushFileBuffers failed (Win32 error "
			+ std::to_string(err) + ")"
		);
	}

	::CloseHandle(hFile);

	BOOL renameOk;

	if (std::filesystem::exists(destination, ec)) {
		renameOk = ::ReplaceFileW(
			destination.wstring().c_str(),
			tempPath.wstring().c_str(),
			nullptr,
			REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS,
			nullptr,
			nullptr
		);
	} else {
		renameOk = ::MoveFileExW(
			tempPath.wstring().c_str(),
			destination.wstring().c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
		);
	}

	if (!renameOk) {
		const DWORD err = ::GetLastError();
		std::filesystem::remove(tempPath, ec);
		return SaveResult::failure(
			"atomicWrite: rename to '"
			+ destination.string() + "' failed (Win32 error "
			+ std::to_string(err) + ")"
		);
	}

#else

	{
		const int fd = ::open(
			tempPath.c_str(),
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
			0644
		);

		if (fd < 0) {
			return SaveResult::failure(
				"atomicWrite: failed to open temp file '"
				+ tempPath.string() + "': " + std::strerror(errno)
			);
		}

		const U8* ptr = data.data();
		size_t remaining = data.size();

		while (remaining > 0) {
			const ssize_t n = ::write(fd, ptr, remaining);

			if (n < 0) {
				if (errno == EINTR)
					continue;

				const int err = errno;
				::close(fd);
				std::filesystem::remove(tempPath, ec);
				return SaveResult::failure(
					"atomicWrite: write to temp file failed: "
					+ std::string(std::strerror(err))
				);
			}

			ptr += n;
			remaining -= static_cast<size_t>(n);
		}

		if (::fsync(fd) != 0) {
			const int err = errno;
			::close(fd);
			std::filesystem::remove(tempPath, ec);
			return SaveResult::failure(
				"atomicWrite: fsync failed: " + std::string(std::strerror(err))
			);
		}

		::close(fd);
	}

	if (::rename(tempPath.c_str(), destination.c_str()) != 0) {
		const int err = errno;
		std::filesystem::remove(tempPath, ec);
		return SaveResult::failure(
			"atomicWrite: rename to '"
			+ destination.string() + "' failed: "
			+ std::string(std::strerror(err))
		);
	}

	{
		const int dirFd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (dirFd >= 0) {
			::fsync(dirFd);
			::close(dirFd);
		}
	}

#endif

	return SaveResult::success();
}

} // namespace Blackthorn::Saves