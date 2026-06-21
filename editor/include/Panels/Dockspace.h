#pragma once

#include "State/Dockspace.h"
#include "State/EditorContext.h"
#include "State/TitleBar.h"

namespace Blackthorn::Editor::Panels {

class Dockspace {
public:
	void draw(
		State::Titlebar& titleBar,
		State::Dockspace& dockspace,
		State::Context& context,
		bool& running
	);
};

} // namespace Blackthorn::Editor::Panels