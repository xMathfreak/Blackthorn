#pragma once

#include <functional>
#include <span>
#include <string>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Saves {

struct BLACKTHORN_API SaveConfig {
	/// Organization name passed to SDL_GetPrefPath() when resolving the save
	/// root directory (see @c directory below). Combined with @c appName to
	/// build a per-user, writable path, e.g.
	/// "%APPDATA%\<orgName>\<appName>\" on Windows. Should stay identical
	/// across all of a studio's titles.
	///
	/// If empty (along with @c appName), pref-path resolution is skipped
	/// entirely and @c directory is resolved relative to the working
	/// directory instead. SaveManager logs a warning when this happens.
	std::string orgName;

	/// Application name passed to SDL_GetPrefPath(). See @c orgName above.
	/// Should be unique per title and never change once chosen.
	std::string appName;

	/// Root directory for LocalFileSaveStorage.
	///
	/// When @c orgName and @c appName are both set, @c directory is treated
	/// as a subfolder appended under SDL_GetPrefPath(orgName, appName), e.g.
	/// a @c directory of "saves" resolves to
	/// "%APPDATA%\MyStudio\MyGame\saves\" on Windows. This guarantees saves
	/// land somewhere writable even if the game itself is installed
	/// somewhere read-only (e.g. under Program Files).
	///
	/// If @c directory is an absolute path, it is used verbatim and
	/// SDL_GetPrefPath() resolution is skipped entirely. Useful for
	/// portable installs, CI, and unit tests that want a fixed, predictable
	/// location.
	std::string directory = "saves";

	/// File extension for save files, including the leading dot.
	/// Must be non-empty and start with '.'.
	/// SaveManger logs an error and falls back to ".sav" if an invalid value is
	/// provided.
	std::string extension = ".sav";

	std::string backupExtension = ".bak";

	/// zstd compression level [1-22]. 0 disables compression.
	int compressionLevel = 3;

	/// Whether to encrypt save files. Defaults to true.
	/// A warning is logged in debug builds when false.
	bool encryptionEnabled = false;

	/// Whether to save when Runtime::shutdown is called.
	bool saveOnShutdown = true;

	/// Whether LocalFileSaveStorage should recover save files that were
	/// renamed outside the engine (e.g. a player renaming "{uuid}.sav" to
	/// "OutsideBossStage.sav" in a file browser).
	///
	/// When enabled, a lookup miss on the canonical "{uuid}.extension" path
	/// triggers a scan of the containing directory for a file whose header
	/// matches the requested SaveID. This applies to reads, existence
	/// checks, removals, and writes. A renamed save keeps being written to
	/// in place on subsequent saves rather than forking into a second,
	/// canonically-named file and to backups as well.
	///
	/// Off by default: it costs a directory scan on every canonical-path
	/// miss, and only takes effect when no custom PathResolver is installed
	/// on the storage backend.
	bool recoverRenamedSaves = false;

	/**
	 * @brief Game-provided key derivation function.
	 *
	 * Required when encryptionEnabled is true. If null and encryption is enabled,
	 * SaveManager will log an error and skip encryption rather than crashing.
	 *
	 * For type safety, prefer using SaveManager::setKeyDeriveFn() directly.
	 */
	std::function<void(std::span<U8, 32>, const void*, U16)> keyDeriveFn;
};

} // namespace Blackthorn::Saves