#pragma once

#include "Core/Export.h"
#include "Debug/Logger.h"

#include <string>

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
	int msaaSamples = 0;
};

struct BLACKTHORN_API JobsConfig {
	size_t workerCount = 0;
};

struct BLACKTHORN_API TimingConfig {
	float fixedDeltaTime = 1.0f / 60.0f;
	int maxFixedUpdates = 10;
	float maxDeltaTime = 0.25f;
	int unfocusedFPS = 10;
};

struct DebugConfig {
	float profilingLogInterval = 1.0f;
	Debug::LoggerConfig logger;
};

struct BLACKTHORN_API EngineConfig {
	WindowConfig window;
	RenderConfig render;
	TimingConfig timing;
	DebugConfig  debug;
	JobsConfig   jobs;

	std::string settingsFilePath = "settings.ini";
};

}