#include "UI/UIManager.h"

#include <algorithm>

#include <SDL3/SDL.h>

#include "UI/Container.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

glm::vec2 UIManager::screenDimensions{1280.0f, 720.0f};
glm::vec2 UIManager::referenceResolution{1280.0f, 720.0f};

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
	effectiveScale = autoScale * globalUIScale;

	updateAllLayouts();
}

void UIManager::setReferenceResolution(float width, float height) {
	if (width <= 0.0f || height <= 0.0f) {
		#ifdef BLACKTHORN_DEBUG
			SDL_Log("UIManager: ignoring invalid reference resolution (%.0f x %.0f)", width, height);
		#endif

		return;
	}

	referenceResolution = {width, height};
	recomputeScale();
}

void UIManager::setGlobalUIScale(float scale) {
	if (scale <= 0.0f) {
		#ifdef BLACKTHORN_DEBUG
			SDL_Log("UIManager: ignoring non-positive UI scale %.2f", scale);
		#endif

		return;
	}

		#ifdef BLACKTHORN_DEBUG
		if (scale < 0.5f || scale > 5.0f)
			SDL_Log("UIManager: extreme global UI scale set: %.2f", scale);
		#endif

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
#ifdef BLACKTHORN_DEBUG
		SDL_Log("UIManager::addWidget — null widget ignored");
#endif
		return;
	}

	root->addWidget(std::move(widget));
}

void UIManager::removeWidget(Widget* widget) {
	if (!widget) {
		#ifdef BLACKTHORN_DEBUG
			SDL_Log("UIManager::removeWidget — null widget ignored");
		#endif

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
	if (root)
		root->update(dt);
}

void UIManager::render(Graphics::Renderer& renderer) {
	if (root)
		root->render(renderer);
}

void UIManager::handleInput(const Input::InputManager& input) {
	// TODO: dispatch mouse/keyboard events from InputManager.
}

} // namespace Blackthorn::UI