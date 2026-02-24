#include "UI/Widgets/Button.h"
#include "Fonts/Font.h"
#include "Graphics/Renderer.h"

namespace Blackthorn::UI {

Button::Button(const std::string& t) {
	setText(t);
}

void Button::render(Graphics::Renderer& renderer) {
	if (!isVisible())
		return;

	glm::vec2 absPos = getAbsolutePosition();
	SDL_FRect rect = {absPos.x, absPos.y, size.x, size.y};

	glm::vec4 bgColor = normalColor;

	if (hasState(state, WidgetState::Disabled)) {
		bgColor = normalColor * 0.5f;
	} else if (hasState(state, WidgetState::Pressed)) {
		bgColor = pressedColor;
	} else if (hasState(state, WidgetState::Hovered)) {
		bgColor = hoverColor;
	}

	if (backgroundTexture && useNineSlice) {
		renderer.drawNineSlice(*backgroundTexture, rect, nineSliceMargins, 0.0f, bgColor);
	} else if (backgroundTexture) {
		renderer.drawTexture(*backgroundTexture, rect, nullptr, 0.0f, 0.0f, bgColor);
	} else {
		renderer.drawQuad(rect, 0.0f, 0.0f, bgColor);
	}

	if (font && !text.empty()) {
		font->drawCached(text, absPos, textScale, 0.0f, size.x, textColor, Text::Alignment::Center);
	}
}

glm::vec2 Button::getMinimumSize() const {
	glm::vec2 minSize{0};

	if (font) {
		auto metrics = font->measure(text, textScale, 0);
		minSize.x = metrics.width + padding.left + padding.right;
		minSize.y = metrics.height + padding.top + padding.bottom;
	}

	return minSize;
}

bool Button::onMouseUp(const glm::vec2& pos, Uint8 button) {
	bool wasPressed = isPressed();
	bool handled = Widget::onMouseUp(pos, button);

	if (wasPressed && handled && containsPoint(pos) && onClick)
		onClick();

	return handled;
}

void Button::setText(const std::string& t) {
	text = t;

	if (widthMode == SizeMode::Content || heightMode == SizeMode::Content) {
		glm::vec2 minSize = getMinimumSize();

		if (widthMode == SizeMode::Content)
			size.x = minSize.x;

		if (heightMode == SizeMode::Content)
			size.y = minSize.y;
	}
}

} //namespace Blackthorn::UI
