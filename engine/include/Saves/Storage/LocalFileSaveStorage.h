#pragma once

#include <filesystem>
#include <functional>

#include "Core/Export.h"
#include "Saves/SaveDocument.h"
#include "Saves/Storage/ISaveStorage.h"

namespace Blackthorn::Saves {

/**
 * @brief File system storage backend for save documents.
 *
 * Writes each save as a single binary file. The file path for a given
 * @c SaveID is determined by a path resolver callable, which defaults to a
 * sensible layout but is fully overridable by the game developer:
 *
 * @par Default path layout
 * @code
 * {rootDir}/{worldID}/{playerID}/{uuid}.sav
 * @endcode
 * If @c worldID or @c playerID are empty they are omitted from the path.
 * A save with neither becomes @c {rootDir}/{uuid}.sav.
 *
 * @par Custom path layout
 * @code
 * storage.setPathResolver([](const SaveID& id) {
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
 *
 * @par Rename recovery
 * If a save file is renamed outside the engine (e.g. a player renaming
 * "{uuid}.sav" to "OutsideBossStage.sav" in a file browser), the default
 * resolver can no longer find it by its canonical UUID-derived path.
 * Constructing with @c recoverRenamedFiles true makes a canonical-path miss
 * fall back to scanning the file's expected directory for a header whose
 * @c saveIDHash matches, recovering both reads and future in-place writes.
 * Disabled by default, and only applies when no custom @c PathResolver is
 * installed. See the constructor docs below.
 */
class BLACKTHORN_API LocalFileSaveStorage final : public ISaveStorage {
public:
	using PathResolver = std::function<std::filesystem::path(const SaveID&)>;

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
	 * @param recoverRenamed When true, and @p res is null, a canonical-path
	 *            miss on @c read()/@c exists()/@c remove()/@c write() falls
	 *            back to scanning the containing directory for a file whose
	 *            @c FileHeader::saveIDHash matches the requested @c SaveID -
	 *            recovering a save that was renamed outside the engine (e.g.
	 *            in a file browser). See @c SaveConfig::recoverRenamedSaves.
	 */
	explicit LocalFileSaveStorage(
		std::filesystem::path root,
		const std::string& ext = ".sav",
		PathResolver res = nullptr,
		bool recoverRenamed = false
	);

	SaveResult write(const SaveID& saveID, const IO::ByteBuffer& data) override;

	SaveReadResult read(const SaveID& saveID) override;

	SaveResult remove(const SaveID& saveID) override;

	bool exists(const SaveID& saveID) override;

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
	bool recoverRenamedFiles;

	/**
	 * @brief Computes the full filesystem path for @p saveID using the
	 * active resolver (or the default layout if none is set).
	 */
	std::filesystem::path resolvePath(const SaveID& saveID) const;

	/**
	 * @brief Resolves the path @c read()/@c exists()/@c remove()/@c write()
	 * should actually operate on for @p saveID, applying rename recovery
	 * when it's enabled and applicable.
	 *
	 * Returns @c resolvePath(saveID) unchanged whenever a custom @c resolver
	 * is installed, @c recoverRenamedFiles is false, or the canonical path
	 * already exists. Otherwise, falls back to @c findRenamedFile() in the
	 * canonical path's parent directory and returns that match if one is
	 * found, so a renamed save keeps resolving to the same file it was
	 * renamed to, including on the next @c write().
	 */
	std::filesystem::path resolveEffectivePath(const SaveID& saveID) const;

	/**
	 * @brief Scans @p dir (non-recursive) for a save file with the configured
	 * extension whose header's @c saveIDHash matches @p saveID.
	 *
	 * Used to recover a save that was renamed outside the engine, e.g. a
	 * player renaming "{uuid}.sav" to "OutsideBossStage.sav" in a file
	 * browser: the canonical UUID-derived path no longer exists, but the
	 * content still identifies itself via the hash stored in its header.
	 *
	 * @param dir Directory to scan. Only this directory is searched, not its
	 *            subdirectories, the same scope @c defaultPath() would have
	 *            placed @p saveID in.
	 * @param saveID Identity being searched for.
	 * @return The matching file's path, or an empty path if none was found.
	 */
	std::filesystem::path findRenamedFile(
		const std::filesystem::path& dir,
		const SaveID& saveID
	) const;

	/**
	 * @brief Reads just the fixed-size @c FileHeader from @p path, without
	 * validating the checksum or reading the rest of the file.
	 *
	 * Used by @c findRenamedFile() to cheaply inspect candidate files (64
	 * bytes each) instead of fully parsing every save in the directory.
	 *
	 * @param path Candidate file to inspect.
	 * @param outHeader Receives the deserialized header on success.
	 * @return true if @p path could be opened, contained at least
	 *         @c FileHeader::SERIALIZED_SIZE bytes, and had a valid magic
	 *         number.
	 */
	static bool tryReadHeader(
		const std::filesystem::path& path,
		FileHeader& outHeader
	);

	/**
	 * @brief Default path resolver. Returns:
	 * @code
	 * {rootDir}/{worldID}/{playerID}/{uuid}.{extension}
	 * @endcode
	 * with empty worldID/playerID segments omitted.
	 */
	std::filesystem::path defaultPath(const SaveID& saveID) const;

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