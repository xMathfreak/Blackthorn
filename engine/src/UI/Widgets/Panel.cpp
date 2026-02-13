#include "UI/Widgets/Panel.h"

#include "Graphics/Renderer.h"

namespace Blackthorn::UI {

Panel::Panel() {}

void Panel::render(Graphics::Renderer& renderer) {
	if (!isVisible())
		return;

	glm::vec2 absPos = getAbsolutePosition();
	renderer.drawQuad({absPos.x, absPos.y, width, height}, 0.0f, 0.0f, backgroundColor);

	Container::render(renderer);
}

} // namespace Blackthorn::UI