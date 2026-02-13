#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn {

namespace Input {
	class InputManager;
}

namespace Graphics {
	class Renderer;
}

namespace Fonts {
	class Font;
}

} // namespace Blackthorn

namespace Blackthorn::UI {

class Widget;

class BLACKTHORN_API UIManager {
private:
	std::vector<std::unique_ptr<Widget>> rootWidgets;

	Widget* focusedWidget = nullptr;
	Widget* hoveredWidget = nullptr;

	Fonts::Font* defaultFont = nullptr;

	Widget* findWidgetAt(const glm::vec2& position);

public:
	UIManager();
	~UIManager();

	void update(float dt);
	void render(Graphics::Renderer& renderer);
	void handleInput(const Input::InputManager& input);

	void addRoot(std::unique_ptr<Widget> widget);
	void removeRoot(Widget* widget);
	void clearRoots();

	void setFocusedWidget(Widget* widget);
	Widget* getFocusedWidget() const { return focusedWidget; }

	void setDefaultFont(Fonts::Font* font) { defaultFont = font; }
	Fonts::Font* getDefaultFont() const { return defaultFont; }
};

} // namespace Blackthorn::UI
