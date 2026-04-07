#include "UI/Widget.h"
#include "UI/Container.h"
#include "UI/UIManager.h"

namespace Blackthorn::UI {

Widget::Widget() {}

void Widget::setState(WidgetState flag) {
	state = state | flag;
	markRenderDirty();
}

void Widget::clearState(WidgetState flag) {
	state = static_cast<WidgetState>(
		static_cast<Uint8>(state) & ~static_cast<Uint8>(flag)
	);
	markRenderDirty();
}

void Widget::toggleState(WidgetState flag) {
	state = state ^ flag;
	markRenderDirty();
}

void Widget::setPosition(const glm::vec2& pos) {
	if (position == pos)
		return;

	position = pos;
	markTransformDirty();
}

glm::vec2 Widget::getAbsolutePosition() const {
	if (transformDirty)
		updateTransform();

	return absolutePosition;
}

void Widget::updateTransform() const {
	const float scale = UIManager::getEffectiveScale();

	glm::vec2 parentAbsPos{0};
	glm::vec2 parentDesignSize{0};

	if (parent) {
		parentAbsPos = parent->getAbsolutePosition();
		glm::vec2 parentSize = parent->getSize();

		parentDesignSize.x = parentSize.x - parent->getPadding().left - parent->getPadding().right;
		parentDesignSize.y = parentSize.y - parent->getPadding().top - parent->getPadding().bottom;
	} else {
		parentDesignSize = UIManager::getScreenDimensions() / scale;
	}

	glm::vec2 alignmentOffset{0};

	switch (alignment.horizontal) {
		case Alignment::Horizontal::Left:
			break;
		case Alignment::Horizontal::Center:
			alignmentOffset.x = (parentDesignSize.x - size.x) * 0.5f;
			break;
		case Alignment::Horizontal::Right:
			alignmentOffset.x = parentDesignSize.x - size.x;
			break;
	}

	switch (alignment.vertical) {
		case Alignment::Vertical::Top:
			break;
		case Alignment::Vertical::Center:
			alignmentOffset.y = (parentDesignSize.y - size.y) * 0.5f;
			break;
		case Alignment::Vertical::Bottom:
			alignmentOffset.y = parentDesignSize.y - size.y;
			break;
	}

	absolutePosition = parentAbsPos + (position + alignmentOffset) * scale;
	transformDirty = false;
}

void Widget::setSize(const glm::vec2& sz) {
	if (size == sz)
		return;

	size = sz;
	designWidth = sz.x;
	designHeight = sz.y;

	markLayoutDirty();
	markTransformDirty();
}

void Widget::setWidth(float w, SizeMode mode) {
	designWidth = w;
	widthMode = mode;

	if (mode == SizeMode::Fixed)
		size.x = w;

	markLayoutDirty();
	markTransformDirty();
}

void Widget::setHeight(float h, SizeMode mode) {
	designHeight = h;
	heightMode = mode;

	if (mode == SizeMode::Fixed) {
		size.y = h;
	}

	markLayoutDirty();
	markTransformDirty();
}

void Widget::setAlignment(const Alignment& align) {
	if (alignment == align)
		return;

	alignment = align;
	markTransformDirty();
}

void Widget::setMargin(const Margin& m) {
	margin = m;
	markLayoutDirty();
}


void Widget::setPadding(const Padding& p) {
	padding = p;
	markLayoutDirty();
	markTransformDirty();
}

void Widget::setParent(Container* container) {
	parent = container;
	markTransformDirty();
	markLayoutDirty();
}

void Widget::setVisible(bool isVisible) {
	if (visible == isVisible)
		return;

	visible = isVisible;

	if (parent)
		parent->markLayoutDirty();

	markRenderDirty();
}

void Widget::setEnabled(bool enabled) {
	if (enabled)
		clearState(WidgetState::Disabled);
	else
		setState(WidgetState::Disabled);
}

void Widget::setFocused(bool focused) {
	if (focused && canFocus())
		setState(WidgetState::Focused);
	else
		clearState(WidgetState::Focused);
}

void Widget::setHovered(bool hovered) {
	if (hovered)
		setState(WidgetState::Hovered);
	else
		clearState(WidgetState::Hovered);
}

bool Widget::onMouseMove(const glm::vec2& pos) {
	bool contains = containsPoint(pos);

	bool wasHovered = isHovered();
	if (contains && !wasHovered) {
		setHovered(true);
	} else if (!contains && wasHovered) {
		setHovered(false);
	}

	return contains;
}

bool Widget::onMouseDown(const glm::vec2& pos, Uint8 button) {
	if (!isEnabled() || !isVisible())
		return false;

	if (containsPoint(pos)) {
		setState(WidgetState::Pressed);
		return true;
	}

	return false;
}

bool Widget::onMouseUp(const glm::vec2& pos, Uint8 button) {
	if (hasState(state, WidgetState::Pressed)) {
		clearState(WidgetState::Pressed);
		return true;
	}

	return false;
}

bool Widget::containsPoint(const glm::vec2& point) const {
	if (!visible)
		return false;

	const float scale = UIManager::getEffectiveScale();
	if (size.x * scale <= 0.0f || size.y * scale <= 0.0f)
		return false;

	glm::vec2 absPos = getAbsolutePosition();

	return point.x >= absPos.x
		&& point.x <= absPos.x + size.x * scale
		&& point.y >= absPos.y
		&& point.y <= absPos.y + size.y * scale;
}

void Widget::markTransformDirty() {
	transformDirty = true;
}

void Widget::markLayoutDirty() {
	layoutDirty = true;
	markTransformDirty();
}

void Widget::markRenderDirty() {
	renderDirty = true;
}

void Widget::updateLayout() {
	layoutDirty = false;
}

} // namespace Blackthorn::UI