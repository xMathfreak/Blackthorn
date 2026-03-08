#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn {

namespace Fonts {
	class Font;
}

namespace Graphics {
	class Renderer;
}

namespace Input {
	class InputManager;
}

namespace UI {

class Widget;
class Container;

class BLACKTHORN_API UIManager {
private:
	static float globalUIScale;
	static Fonts::Font* defaultFont;
	static glm::vec2 screenDimensions;
	static std::vector<UIManager*> managers;

	static void updateAllLayouts();

private:
	Widget* focusedWidget = nullptr;
	Widget* hoveredWidget = nullptr;

	std::unique_ptr<Container> root;

	void updateLayout();
	Widget* findWidgetAt(const glm::vec2& position);

public:
	UIManager();
	~UIManager();

	UIManager(const UIManager&) = delete;
	UIManager& operator=(const UIManager&) = delete;

	UIManager(UIManager&&) = delete;
	UIManager& operator=(UIManager&&) = delete;

	/**
	 * @brief Called when the window is resized
	 * Updates screen dimensions and triggers layout updates for all managers
	 */
	static void onWindowResize(int width, int height);

	/**
	 * @brief Set global UI scale factor
	 * Affects all widgets across all UIManager instances
	 * @param scale Scale factor (typically 1.0-2.0)
	 */
	static void setGlobalUIScale(float scale);
	static float getGlobalUIScale() { return globalUIScale; }

	/**
	 * @brief Get current screen dimensions
	 */
	static glm::vec2 getScreenDimensions() { return screenDimensions; }

	/**
	 * @brief Set default font for widgets that don't specify one
	 */
	static void setDefaultFont(Fonts::Font* font);
	static Fonts::Font* getDefaultFont() { return defaultFont; }

	/**
	 * @brief Get number of active UIManager instances
	 */
	static size_t getManagerCount() { return managers.size(); }

	/**
	 * @brief Add a widget to the root container
	 * @param widget Widget to add (ownership transferred)
	 */
	void addWidget(std::unique_ptr<Widget> widget);

	/**
	 * @brief Remove a widget from the root container
	 * @param widget Pointer to widget to remove
	 */
	void removeWidget(Widget* widget);

	/**
	 * @brief Remove all widgets from the root container
	 */
	void clearWidgets();

	/**
	 * @brief Get the root container
	 */
	Container* getRoot() const { return root.get(); }

	/**
	 * @brief Update all widgets
	 * @param dt Delta time in seconds
	 */
	void update(float dt);

	/**
	 * @brief Render all widgets
	 * @param renderer Renderer to use for drawing
	 */
	void render(Graphics::Renderer& renderer);

	/**
	 * @brief Handle input events
	 * @param input Input manager with current input state
	 */
	void handleInput(const Input::InputManager& input);

	/**
	 * @brief Set the currently focused widget
	 * Unfocuses previous widget if any
	 * @param widget Widget to focus (nullptr to clear focus)
	 */
	void setFocusedWidget(Widget* widget);

	/**
	 * @brief Get the currently focused widget
	 */
	Widget* getFocusedWidget() const { return focusedWidget; }

	/**
	 * @brief Set the currently hovered widget
	 * Un-hovers previous widget if any
	 * @param widget Widget being hovered (nullptr to clear hover)
	 */
	void setHoveredWidget(Widget* widget);

	/**
	 * @brief Get the currently hovered widget
	 */
	Widget* getHoveredWidget() const { return hoveredWidget; }
};

} // namespace UI

} // namespace Blackthorn