#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "Core/Export.h"
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
	Uint32 maxQuads = 4096;

	static constexpr Uint32 maxTextureSlots = 16;
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
	Uint32 maxCachedText = 256;
	Uint32 maxTextGlyphs = 2048;
	int atlasSize = 1024;
	Uint32 tabSpaces = 4;

	static void setCurrent(const FontConfig& cfg);
	static const FontConfig& getCurrent();

private:
	static FontConfig current;
};

struct BLACKTHORN_API AssetConfig {
	/// Maximum number of async asset uploads per frame.
	size_t uploadBudget = 4;
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
	Uint32 pollIntervalMicros = 500;

	/// Idle time before a TCP peer is probed with a Heartbeat, in ms.
	/// Set to 0 to disable. Default: 5000ms (half the default timeout).
	Uint32 heartbeatIntervalMs = 5000;

	/// UDP port to bind on (server and client). 0 = OS-assigned ephemeral.
	Uint16 udpPort = 7777;

	/// TCP port to listen on (server only). 0 = disabled.
	Uint16 tcpPort = 7778;

	/// When false, UDP datagrams from unknown addresses are silently dropped.
	bool allowUDPImplicitPeers = true;
};

struct BLACKTHORN_API EngineConfig {
	DebugConfig debug;
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