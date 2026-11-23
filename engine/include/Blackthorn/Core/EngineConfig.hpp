#pragma once

#include <filesystem>
#include <string>

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Types/Numeric.hpp"

#include "Blackthorn/Assets/AssetConfig.hpp"
#include "Blackthorn/Core/MetadataConfig.hpp"
#include "Blackthorn/Debug/DebugConfig.hpp"
#include "Blackthorn/Audio/AudioConfig.hpp"
#include "Blackthorn/Fonts/FontConfig.hpp"
#include "Blackthorn/Graphics/RenderConfig.hpp"
#include "Blackthorn/Net/ConnectionConfig.hpp"
#include "Blackthorn/Saves/SaveConfig.hpp"

namespace Blackthorn {

struct BLACKTHORN_API WindowConfig {
	std::string title = "Blackthorn Engine";
	int width = 640;
	int height = 480;
	bool resizable = true;
};

struct BLACKTHORN_API ThreadingConfig {
	/// Number of worker threads for the job system.
	/// 0 = auto `(max(1, thread::hardware_concurrency - 1))`.
	/// Values above `thread::hardware_concurrency` can lead to
	/// performance degradation.
	size_t jobWorkerCount = 0;
};

struct BLACKTHORN_API TimingConfig {
	float fixedDeltaTime = 1.0f / 60.0f;
	int maxFixedUpdates = 10;
	float maxDeltaTime = 0.25f;
	U64 initialTick = 0;
};

struct BLACKTHORN_API EngineConfig {
	MetadataConfig metadata;
	Saves::SaveConfig save;
	Debug::DebugConfig debug;
	Net::ConnectionConfig net;
	WindowConfig window;
	TimingConfig timing;
	Graphics::RenderConfig render;
	Fonts::FontConfig fonts;
	Audio::AudioConfig audio;
	ThreadingConfig threading;
	Assets::AssetConfig assets;

	std::filesystem::path settingsFilePath = "settings.toml";
};

}