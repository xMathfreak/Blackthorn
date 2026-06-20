#pragma once

#include "State/Dockspace.h"
#include "State/TitleBar.h"

namespace Blackthorn::Editor::Panels {

class Dockspace {
public:
	void draw(
		State::Titlebar& titleBar,
		State::Dockspace& dockspace,
		bool& running
	);
};

} // namespace Blackthorn::Editor::Panels