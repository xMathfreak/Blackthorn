#pragma once

#include <SDL3/SDL.h>

#include "State/EditorContext.h"
#include "State/TitleBar.h"

namespace Blackthorn::Editor::Panels {

class TitleBar {
public:
	void draw(
		SDL_Window* window,
		bool& running,
		State::Titlebar& titlebar,
		State::Context& context
	);
};

} // namespace Blackthorn::Editor::Panels