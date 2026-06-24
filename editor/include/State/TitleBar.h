#pragma once

#include <vector>

#include <SDL3/SDL.h>

namespace Blackthorn::Editor::State {

struct Titlebar {
	float height = 36.0f;
	float buttonWidth = 54.0f;

	std::vector<SDL_Rect> hitExclusionRects;
};

} // namespace Blackthorn::Editor::State