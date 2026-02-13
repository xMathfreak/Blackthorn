#include "UI/Widget.h"
#include "UI/Container.h"

namespace Blackthorn::UI {

Widget::Widget() {}

void Widget::update(float dt) {}

void Widget::render(Graphics::Renderer& renderer) {}

bool Widget::onMouseMove(const glm::vec2& pos) {
	updateHoverState(pos);
	return containsPoint(pos);
}

bool Widget::onMouseDown(const glm::vec2& pos, Uint8 button) {
	if (!isEnabled() || !isVisible())
		return false;

	if (containsPoint(pos)) {
		state = state | WidgetState::Pressed;
		return true;
	}

	return false;
}

bool Widget::onMouseUp(const glm::vec2& pos, Uint8 button) {
	if (hasState(state, WidgetState::Pressed)) {
		state = static_cast<WidgetState>(
			static_cast<int>(state) & ~static_cast<int>(WidgetState::Pressed)
		);

		return true;
	}

	return false;
}

bool Widget::onKeyDown(SDL_Keycode key) {
	return false;
}

bool Widget::onKeyUp(SDL_Keycode key) {
	return false;
}

void Widget::setPosition(const glm::vec2& pos) {
	position = pos;
}

void Widget::setSize(const glm::vec2& s) {
	size = s;
	width = size.x;
	height = size.y;
}

glm::vec2 Widget::getAbsolutePosition() const {
	glm::vec2 absolutePosition = position;

	if (parent) {
		absolutePosition += parent->getAbsolutePosition();
		absolutePosition += glm::vec2(parent->getPadding().x, parent->getPadding().y);
	}

	return absolutePosition;
}

void Widget::setWidth(float w, SizeMode mode) {
	width = w;
	widthMode = mode;

	if (mode == SizeMode::Fixed)
		size.x = width;
}

void Widget::setHeight(float h, SizeMode mode) {
	height = h;
	heightMode = mode;

	if (mode == SizeMode::Fixed)
		size.y = height;
}

void Widget::setEnabled(bool enabled) {
	if (enabled) {
		state = static_cast<WidgetState>(
			static_cast<int>(state) & ~static_cast<int>(WidgetState::Disabled)
		);
	} else {
		state = state | WidgetState::Disabled;
	}
}

void Widget::setFocused(bool focused) {
	if (focused && canFocus()) {
		state = state | WidgetState::Focused;
	} else {
		state = static_cast<WidgetState>(
			static_cast<int>(state) & ~static_cast<int>(WidgetState::Focused)
		);
	}
}

bool Widget::containsPoint(const glm::vec2& point) const {
	glm::vec2 absPos = getAbsolutePosition();

	return point.x >= absPos.x && point.x <= absPos.x + size.x
		&& point.y >= absPos.y && point.y <= absPos.y + size.y;
}

void Widget::setMargin(float m) {
	margin = glm::vec4(m);
}

void Widget::setMargin(float top, float right, float bottom, float left) {
	margin = {top, right, bottom, left};
}

void Widget::setMargin(const glm::vec4& m) {
	margin = m;
}

void Widget::setPadding(float p) {
	padding = glm::vec4(p);
}

void Widget::setPadding(float top, float right, float bottom, float left) {
	padding = {top, right, bottom, left};
}

void Widget::setPadding(const glm::vec4& p) {
	padding = p;
}

void Widget::updateHoverState(const glm::vec2& mousePos) {
	if (!isEnabled() || !isVisible())
		return;

	bool wasHovered = hasState(state, WidgetState::Hovered);
	bool nowHovered = containsPoint(mousePos);

	if (nowHovered && !wasHovered) {
		state = state | WidgetState::Hovered;
	} else if (!nowHovered && wasHovered) {
		state = static_cast<WidgetState>(
			static_cast<int>(state) & ~static_cast<int>(WidgetState::Hovered)
		);
	}
}


} // namespace Blackthorn::UI
