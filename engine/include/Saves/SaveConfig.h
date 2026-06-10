#pragma once

#include <functional>
#include <span>
#include <string>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Saves {

struct BLACKTHORN_API SaveConfig {
	/// Root directory for LocalaveFileStorage.
	/// Relative paths are resolved from the working directory.
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

	/// Whether to save when EngineCore::shutdown is called.
	bool saveOnShutdown = true;

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