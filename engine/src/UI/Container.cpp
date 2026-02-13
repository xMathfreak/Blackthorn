#include "UI/Container.h"

#include <algorithm>

namespace Blackthorn::UI {

Container::Container() {}

void Container::addChild(std::unique_ptr<Widget> child) {
	child->setParent(this);
	children.push_back(std::move(child));
	layoutChildren();
}

void Container::removeChild(Widget* child) {
	auto it = std::find_if(children.begin(), children.end(),
		[child](const std::unique_ptr<Widget>& ptr) {
			return ptr.get() == child;
		}
	);

	if (it != children.end()) {
		children.erase(it);
		layoutChildren();
	}
}

void Container::clearChildren() {
	children.clear();
}

void Container::update(float dt) {
	Widget::update(dt);

	for (auto& child : children) {
		if (child->isVisible())
			child->update(dt);
	}
}

void Container::render(Graphics::Renderer& renderer) {
	if (!isVisible())
		return;

	Widget::render(renderer);

	for (auto& child : children) {
		if (child->isVisible())
			child->render(renderer);
	}
}

bool Container::onMouseMove(const glm::vec2& pos) {
	if (!isEnabled() || !isVisible())
		return false;

	Widget::onMouseMove(pos);

	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if ((*it)->onMouseMove(pos))
			return true;
	}

	return containsPoint(pos);
}

bool Container::onMouseDown(const glm::vec2& pos, Uint8 button) {
	if (!isEnabled() || !isVisible())
		return false;

	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if ((*it)->onMouseDown(pos, button))
			return true;
	}

	return Widget::onMouseDown(pos, button);
}

bool Container::onMouseUp(const glm::vec2& pos, Uint8 button) {
	if (!isEnabled() || !isVisible())
		return false;

	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if ((*it)->onMouseUp(pos, button))
			return true;
	}

	return Widget::onMouseUp(pos, button);
}

bool Container::onKeyDown(SDL_Keycode key) {
	for (auto& child : children) {
		if (child->isFocused())
			return child->onKeyDown(key);
	}

	return Widget::onKeyDown(key);
}

bool Container::onKeyUp(SDL_Keycode key) {
	for (auto& child : children) {
		if (child->isFocused())
			return child->onKeyUp(key);
	}

	return Widget::onKeyUp(key);
}

void Container::layoutChildren() {}

Widget* Container::getChildAt(const glm::vec2& pos) {
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if ((*it)->containsPoint(pos))
			return it->get();
	}

	return nullptr;
}

} //namespace Blackthorn::UI