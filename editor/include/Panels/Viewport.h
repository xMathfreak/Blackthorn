#pragma once

#include "State/Context.h"
#include "State/Viewport.h"

namespace Blackthorn {

namespace Graphics {
	class Renderer;
} // namespace Graphics

namespace Editor::Panels {

class Viewport {
public:
	void draw(
		State::Context& context,
		Graphics::Renderer& renderer,
		State::Viewport& viewport,
		float alpha
	);
};

} // namespace Editor::Panels

} // namespace Blackthorn