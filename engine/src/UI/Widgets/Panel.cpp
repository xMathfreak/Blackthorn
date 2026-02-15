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

void Panel::layoutChildren() {
	Container::layoutChildren();

	if (autoResize && layout) {
		glm::vec2 minSize = layout->calculateMinimumSize(this);

		if (widthMode == SizeMode::Content)
			size.x = minSize.x;

		if (heightMode == SizeMode::Content)
			size.y = minSize.y;
	}
}

} // namespace Blackthorn::UI