#pragma once

#include <filesystem>
#include <functional>

#include "Core/Export.h"
#include "Saves/Storage/ISaveStorage.h"

namespace Blackthorn::Saves {

/**
 * @brief File system storage backend for save documents.
 *
 * Writes each save as a single binary file. The file path for a given
 * @c SaveId is determined by a path resolver callable, which defaults to a
 * sensible layout but is fully overridable by the game developer:
 *
 * @par Default path layout
 * @code
 * {rootDir}/{worldId}/{playerId}/{uuid}.sav
 * @endcode
 * If @c worldId or @c playerId are empty they are omitted from the path.
 * A save with neither becomes @c {rootDir}/{uuid}.sav.
 *
 * @par Custom path layout
 * @code
 * storage.setPathResolver([](const SaveId& id) {
 *		return std::filesystem::path("my_saves") / (id.slot == 0
 *			 ? id.id.toString() + ".sav"
 *			 : "slot_" + std::to_string(id.slot) + ".sav");
 * });
 * @endcode
 *
 * @par Listing
 * @c list() scans the root directory recursively for @c *.sav files,
 * parses their unencrypted headers and section tables via @c SaveDocument::parse(),
 * and applies the filter. Files that fail to parse are logged and skipped.
 */
class BLACKTHORN_API LocalFileSaveStorage final : public ISaveStorage {
public:
	using PathResolver = std::function<std::filesystem::path(const SaveId&)>;

	/**
	 * @brief Constructs the storage backend.
	 *
	 * @param root Root directory under which save files are written. Created
	 *             automatically on first write if absent.
	 * @param extension File extension for save files, including the leading
	 *                  dot (e.g. @c ".sav"). Used by the default path resolver
	 *                  and by @c list() when scanning for files. Ignored when
	 *                  a custom resolver is active.
	 * @param res Optional custom path resolver. If null the default layout is
	 *            used.
	 */
	explicit LocalFileSaveStorage(
		std::filesystem::path root,
		const std::string& ext = ".sav",
		PathResolver res = nullptr
	);

	SaveResult write(const SaveId& saveId, const IO::ByteBuffer& data) override;

	SaveReadResult read(const SaveId& saveId) override;

	SaveResult remove(const SaveId& saveId) override;

	bool exists(const SaveId& saveId) override;

	std::vector<SaveMetadata> list(const SaveFilter& filter) override;

	/**
	 * @brief Replaces the path resolver at runtime.
	 * Takes effect on the next storage operation.
	 */
	void setPathResolver(PathResolver resolver);

	const std::filesystem::path& getRootDir() const noexcept { return rootDir; }

	const std::string& getExtension() const noexcept { return extension; }

private:
	std::filesystem::path rootDir;
	std::string extension;
	PathResolver resolver;

	/**
	 * @brief Computes the full filesystem path for @p saveId using the
	 * active resolver (or the default layout if none is set).
	 */
	std::filesystem::path resolvePath(const SaveId& saveId) const;

	/**
	 * @brief Default path resolver. Returns:
	 * @code
	 * {rootDir}/{worldId}/{playerId}/{uuid}.{extension}
	 * @endcode
	 * with empty worldId/playerId segments omitted.
	 */
	std::filesystem::path defaultPath(const SaveId& saveId) const;

	/**
	 * @brief Writes @p data atomically to @p destination.
	 *
	 * Procedure:
	 *   1. Write to a sibling temp file in the same directory as @p destination
	 *      (guarantees same filesystem, so rename is atomic).
	 *   2. fsync the temp file so data reaches the storage medium.
	 *   3. Rename the temp file over @p destination (atomic on POSIX; uses
	 *      ReplaceFileW on Windows for equivalent semantics).
	 *   4. fsync the parent directory (POSIX only) so the directory entry is
	 *      durable.
	 *
	 *        The temp file is named @c "<destination>.tmp.<pid>" to avoid
	 *        collisions when multiple processes write the same save
	 *        simultaneously (e.g. a game instance and an editor).
	 *
	 * @param destination Final target path.
	 * @param data  Bytes to write.
	 *
	 * @return @c SaveResult::success() or a descriptive failure.
	 */
	static SaveResult atomicWrite(
		const std::filesystem::path& destination,
		const IO::ByteBuffer& data
	);

	/**
	 * @brief Returns the number of bytes available on the filesystem that
	 *        contains @p path.
	 *
	 *        Returns 0 on any error (treat as insufficient space).
	 *
	 * @param path Any path on the target filesystem
	 */
	static UMAX availableBytes(const std::filesystem::path& path);
};

} // namespace Blackthorn::Saves