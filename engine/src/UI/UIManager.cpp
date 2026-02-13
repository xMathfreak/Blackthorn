#include "UI/UIManager.h"

#include <algorithm>

#include "Input/InputManager.h"
#include "UI/Container.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

UIManager::UIManager() {}

UIManager::~UIManager() = default;

void UIManager::update(float dt) {
	for (auto& widget : rootWidgets) {
		if (widget->isVisible())
			widget->update(dt);
	}
}

void UIManager::render(Graphics::Renderer& renderer) {
	for (auto& widget : rootWidgets) {
		if (widget->isVisible())
			widget->render(renderer);
	}
}

void UIManager::addRoot(std::unique_ptr<Widget> widget) {
	rootWidgets.push_back(std::move(widget));
}

void UIManager::removeRoot(Widget* widget) {
	auto it = std::find_if(rootWidgets.begin(), rootWidgets.end(),
		[widget](const std::unique_ptr<	Widget>& ptr) {
			return ptr.get() == widget;
		}
	);

	if (it != rootWidgets.end())
		rootWidgets.erase(it);
}

void UIManager::clearRoots() {
	rootWidgets.clear();
	focusedWidget = nullptr;
	hoveredWidget = nullptr;
}

void UIManager::setFocusedWidget(Widget* widget) {
	if (focusedWidget)
		focusedWidget->setFocused(false);

	focusedWidget = widget;

	if (focusedWidget)
		focusedWidget->setFocused(true);
}

void UIManager::handleInput(const Input::InputManager& input) {
	glm::vec2 mousePos = input.getMousePosition();

	for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
		if ((*it)->isVisible())
			(*it)->onMouseMove(mousePos);
	}

	if (input.isMouseButtonPressed(SDL_BUTTON_LEFT)) {
		for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
			if ((*it)->isVisible() && (*it)->onMouseDown(mousePos, SDL_BUTTON_LEFT)) {
				Widget* clickedWidget = findWidgetAt(mousePos);

				if (clickedWidget && clickedWidget->canFocus())
					setFocusedWidget(clickedWidget);

				return;
			}
		}

		setFocusedWidget(nullptr);
	}

	if (input.isMouseButtonReleased(SDL_BUTTON_LEFT)) {
		for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
			if ((*it)->isVisible())
				(*it)->onMouseUp(mousePos, SDL_BUTTON_LEFT);
		}
	}

	if (input.isMouseButtonPressed(SDL_BUTTON_RIGHT)) {
		for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
			if ((*it)->isVisible() && (*it)->onMouseDown(mousePos, SDL_BUTTON_RIGHT))
				return;
		}
	}

	if (input.isMouseButtonReleased(SDL_BUTTON_RIGHT)) {
		for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
			if ((*it)->isVisible())
				(*it)->onMouseUp(mousePos, SDL_BUTTON_RIGHT);
		}
	}

	// Handle keyboard input
}

Widget* UIManager::findWidgetAt(const glm::vec2& position) {
	for (auto it = rootWidgets.rbegin(); it != rootWidgets.rend(); ++it) {
		Widget* widget = it->get();

		if (!widget->isVisible())
			continue;

		if (auto* container = dynamic_cast<Container*>(widget)) {
			std::function<Widget*(Container*, const glm::vec2&)> searchContainer
			= [&](Container* cont, const glm::vec2& pos) -> Widget* {
				for (auto containerIt = cont->getChildren().rbegin();
					containerIt != cont->getChildren().rend(); ++containerIt) {
					Widget* child = containerIt->get();

					if (!child->isVisible())
						continue;

					if (auto* childContainer = dynamic_cast<Container*>(child)) {
						Widget* found = searchContainer(childContainer, pos);

						if (found)
							return found;
					}

					if (child->containsPoint(pos))
						return child;
				}

				return nullptr;
			};

			Widget* found = searchContainer(container, position);
			if (found)
				return found;
		}

		if (widget->containsPoint(position))
			return widget;
	}

	return nullptr;
}

} // namespace Blackthorn::UI
