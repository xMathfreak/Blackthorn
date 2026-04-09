#include "UI/UIManager.h"

#include <algorithm>

#include <SDL3/SDL.h>

#include "Debug/Logger.h"
#include "Input/InputManager.h"
#include "UI/Container.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

glm::vec2 UIManager::screenDimensions{1280.0f, 720.0f};
glm::vec2 UIManager::referenceResolution{1280.0f, 720.0f};

bool UIManager::layoutPending = false;
float UIManager::autoScale = 1.0f;
float UIManager::globalUIScale = 1.0f;
float UIManager::effectiveScale = 1.0f;

Fonts::Font* UIManager::defaultFont = nullptr;
std::vector<UIManager*> UIManager::managers;

UIManager::UIManager() {
	root = std::make_unique<Container>();
	root->setSizingMode(Container::SizingMode::FillParent);

	managers.push_back(this);
}

UIManager::~UIManager() {
	auto it = std::find(managers.begin(), managers.end(), this);
	if (it != managers.end())
		managers.erase(it);
}

void UIManager::onWindowResize(int width, int height) {
	screenDimensions.x = static_cast<float>(width);
	screenDimensions.y = static_cast<float>(height);

	recomputeScale();
}

void UIManager::recomputeScale() {
	float scaleX = screenDimensions.x / referenceResolution.x;
	float scaleY = screenDimensions.y / referenceResolution.y;

	autoScale = std::min(scaleX, scaleY);
	effectiveScale = std::max(1.0f, autoScale * globalUIScale);

	layoutPending = true;
}

void UIManager::flushPendingLayout() {
	if (!layoutPending)
		return;

	layoutPending = false;
	updateAllLayouts();
}

void UIManager::setReferenceResolution(float width, float height) {
	if (width <= 0.0f || height <= 0.0f) {
		BT_WARN("UIManager: Invalid reference resolution ({:.0f} x {:.0f}). Values must be positive, request ignored", width, height);
		return;
	}

	referenceResolution = {width, height};
	recomputeScale();
}

void UIManager::setGlobalUIScale(float scale) {
	if (scale <= 0.0f) {
		BT_WARN("UIManager: Ignoring non-positive global UI scale: {:.2f}", scale);
		return;
	}

	if (scale < 0.5f || scale > 5.0f)
		BT_WARN("UIManager: Extreme global UI scale set: {:.2f}", scale);

	globalUIScale = scale;
	recomputeScale();
}

void UIManager::setDefaultFont(Fonts::Font* font) {
	defaultFont = font;
}

void UIManager::updateAllLayouts() {
	for (auto* m : managers) {
		if (m)
			m->updateLayout();
	}
}

void UIManager::addWidget(std::unique_ptr<Widget> widget) {
	if (!widget) {
		BT_WARN("UIManager::addWidget() received nullptr, request ignored");
		return;
	}

	root->addWidget(std::move(widget));
}

void UIManager::removeWidget(Widget* widget) {
	if (!widget) {
		BT_WARN("UIManager::removeWidget() received nullptr, request ignored");
		return;
	}

	root->removeWidget(widget);
}

void UIManager::clearWidgets() {
	root->clearWidgets();
	focusedWidget = nullptr;
	hoveredWidget = nullptr;
}

void UIManager::updateLayout() {
	if (!root)
		return;

	root->markLayoutDirty();
	root->markTransformDirty();

	root->updateLayout();
}

void UIManager::setFocusedWidget(Widget* widget) {
	if (focusedWidget == widget)
		return;

	if (focusedWidget)
		focusedWidget->setFocused(false);

	focusedWidget = widget;

	if (focusedWidget)
		focusedWidget->setFocused(true);
}

void UIManager::setHoveredWidget(Widget* widget) {
	if (hoveredWidget == widget)
		return;

	if (hoveredWidget)
		hoveredWidget->setHovered(false);

	hoveredWidget = widget;

	if (hoveredWidget)
		hoveredWidget->setHovered(true);
}

Widget* UIManager::findWidgetAt(const glm::vec2& position) {
	if (!root)
		return nullptr;

	std::function<Widget*(Container*, const glm::vec2&)> searchContainer;
	searchContainer = [&](Container* container, const glm::vec2& pos) -> Widget* {
		if (!container || !container->isVisible())
			return nullptr;

		const auto& childWidgets = container->getWidgets();

		for (auto it = childWidgets.rbegin(); it != childWidgets.rend(); ++it) {
			Widget* widget = it->get();
			if (!widget || !widget->isVisible())
				continue;

			if (auto* childContainer = dynamic_cast<Container*>(widget)) {
				if (Widget* found = searchContainer(childContainer, pos))
					return found;
			}

			if (widget->containsPoint(pos))
				return widget;
		}

		return nullptr;
	};

	return searchContainer(root.get(), position);
}

void UIManager::update(float dt) {
	flushPendingLayout();

	if (root)
		root->update(dt);
}

void UIManager::render(Graphics::Renderer& renderer) {
	if (root)
		root->render(renderer);
}

void UIManager::handleInput(const Input::InputManager& input) {
	if (!root)
		return;

	const glm::vec2 mousePos = input.getMousePosition();

	if (root)
		root->onMouseMove(mousePos);

	Widget* widgetUnderCursor = findWidgetAt(mousePos);
	setHoveredWidget(widgetUnderCursor);

	for (Uint8 btn = 1; btn <= 3; ++btn) {
		if (input.isMouseButtonPressed(btn)) {
			if (root->onMouseDown(mousePos, btn)) {
				Widget* clicked = findWidgetAt(mousePos);

				if (clicked && clicked->canFocus()) {
					setFocusedWidget(clicked);
				} else {
					setFocusedWidget(nullptr);
				}
			}
		}

		if (input.isMouseButtonReleased(btn))
			root->onMouseUp(mousePos, btn);
	}

	if (!focusedWidget)
		return;

	static constexpr SDL_Keycode uiKeys[] = {
		SDLK_RETURN, SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_DELETE,
		SDLK_TAB, SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN,
		SDLK_HOME, SDLK_END, SDLK_PAGEUP, SDLK_PAGEDOWN,
		SDLK_SPACE
	};

	for (SDL_Keycode key : uiKeys) {
		if (input.isKeyPressed(key))
			focusedWidget->onKeyDown(key);

		if (input.isKeyReleased(key))
			focusedWidget->onKeyUp(key);
	}

	for (SDL_Keycode key = 32; key <= 126; ++key) {
		if (input.isKeyPressed(key))
			focusedWidget->onKeyDown(key);

		if (input.isKeyReleased(key))
			focusedWidget->onKeyUp(key);
	}
}

} // namespace Blackthorn::UI