#pragma once

#include <functional>
#include <span>
#include <string>

#include "Core/Export.h"
#include "Core/Types/Types.h"
#include "Debug/Logger.h"
#include "Net/Connection/PeerRateLimiter.h"

namespace Blackthorn {

struct BLACKTHORN_API WindowConfig {
	std::string title = "Blackthorn Engine";
	int width = 1280;
	int height = 720;
	bool resizable = true;
};

struct BLACKTHORN_API RenderConfig {
	int openglMajor = 3;
	int openglMinor = 3;
	int depthBits = 16;
	int stencilBits = 0;

	/// Maximum number of quads per batch.
	/// Drives MAX_VERTICES `(maxQuads * 4)` and
	/// MAX_INDICES `(maxQuads * 6)` inside the Renderer.
	U32 maxQuads = 4096;

	static constexpr U32 maxTextureSlots = 16;
};

struct BLACKTHORN_API ThreadingConfig {
	/// Number of worker threads for the job system.
	/// 0 = auto `(max(1, thread::hardware_concurrency - 1))`.
	/// Values above `thread::hardware_concurrency` can lead to
	/// performance degradation.
	size_t jobWorkerCount = 0;

	/// Number of worker threads for the asset manager.
	/// 0 = auto `(max(1, thread::hardware_concurrency - 1))`.
	size_t assetWorkerCount = 0;
};

struct BLACKTHORN_API TimingConfig {
	float fixedDeltaTime = 1.0f / 60.0f;
	int maxFixedUpdates = 10;
	float maxDeltaTime = 0.25f;
	int unfocusedFPS = 10;
};

struct BLACKTHORN_API DebugConfig {
	float profilingLogInterval = 1.0f;
	Debug::LoggerConfig logger;
};

struct BLACKTHORN_API FontConfig {
	U32 maxCachedText = 256;
	U32 maxTextGlyphs = 2048;
	int atlasSize = 1024;
	U32 tabSpaces = 4;

	static void setCurrent(const FontConfig& cfg);
	static const FontConfig& getCurrent();

private:
	static FontConfig current;
};

struct BLACKTHORN_API AssetConfig {
	/// Maximum number of async asset uploads per frame.
	size_t uploadBudget = 4;
};

struct BLACKTHORN_API SaveConfig {
	/// Root directory for LocalaveFileStorage.
	/// Relative paths are resolved from the working directory.
	std::string directory = "saves";

	/// File extension for save files, including the leading dot.
	/// Must be non-empty and start with '.'.
	/// SaveManger logs an error and falls back to ".sav" if an invalid value is
	/// provided.
	std::string extension = ".sav";

	/// zstd compression level [1-22]. 0 disables compression.
	int compressionLevel = 3;

	/// Whether to encrypt save files. Defaults to true.
	/// A warning is logged in debug builds when false.
	bool encryptionEnabled = true;

	/**
	 * @brief Game-provided key derivation function.
	 *
	 * Required when encryptionEnabled is true. If null and encryption is enabled,
	 * SaveManager will log an error and skip encryption rather than crashing.
	 *
	 * The intended signature is:
	 * `std::function<void(std::span<U8, 32>, const Saves::SaveId&, U16)>`
	 *
	 * However, it is stored as `void*` here to avoid pulling Saves headers into
	 * EngineConfig. SaveManager casts it back to the correct type on construction.
	 *
	 * For type safety, prefer using SaveManager::setKeyDeriveFn() directly.
	 */
	std::function<void(std::span<U8, 32>, const void*, U16)> keyDeriveFn;
};

/**
 * @brief Configuration passed to @c ConnectionManager::start().
 */
struct BLACKTHORN_API ConnectionConfig {
	/// Default rate-limit config applied to every new peer.
	Net::Connection::RateLimitConfig rateLimitDefaults = Net::Connection::RateLimitConfig{};

	/// Maximum number of simultaneous peers.
	size_t maxPeers = 64;

	/// Capacity of the inbound packet queue. Must be a power of two.
	size_t queueCapacity = 256;

	/// I/O thread poll interval in microseconds. Default: 500µs.
	U32 pollIntervalMicros = 500;

	/// Idle time before a TCP peer is probed with a Heartbeat, in ms.
	/// Set to 0 to disable. Default: 5000ms (half the default timeout).
	U32 heartbeatIntervalMs = 5000;

	/// UDP port to bind on (server and client). 0 = OS-assigned ephemeral.
	U16 udpPort = 7777;

	/// TCP port to listen on (server only). 0 = disabled.
	U16 tcpPort = 7778;

	/// When false, UDP datagrams from unknown addresses are silently dropped.
	bool allowUDPImplicitPeers = true;
};

struct BLACKTHORN_API EngineConfig {
	DebugConfig debug;
	SaveConfig save;
	ConnectionConfig net;
	WindowConfig window;
	RenderConfig render;
	TimingConfig timing;
	FontConfig fonts;
	ThreadingConfig threading;
	AssetConfig assets;

	std::string settingsFilePath = "settings.ini";
};

}