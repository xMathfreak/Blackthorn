#pragma once

#include "Core/Export.h"
#include <string>

namespace Blackthorn {

struct BLACKTHORN_API WindowConfig {
	std::string title = "Blackthorn Engine";
	int width = 1280;
	int height = 720;
	bool vsync = true;
	bool resizable = true;
	bool fullscreen = false;
};

struct BLACKTHORN_API RenderConfig {
	int openglMajor = 3;
	int openglMinor = 3;
};

struct BLACKTHORN_API TimingConfig {
	float fixedDeltaTime = 1.0f / 60.0f;
	float maxDeltaTime = 0.25f;
	bool capFrameRate = false;
	int targetFPS = 60;
};

struct BLACKTHORN_API EngineConfig {
	WindowConfig window;
	RenderConfig render;
	TimingConfig timing;
};

}