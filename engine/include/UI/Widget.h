#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace Blackthorn {

namespace Graphics {
	class Renderer;
}

namespace Fonts {
	class Font;
}

} // namespace Blackthorn

namespace Blackthorn::UI {

class Container;
class Widget;

enum class Align {
	Start,
	Center,
	End,
	Stretch
};

enum class SizeMode {
	Fixed,
	Content,
	Percent
};

enum class WidgetState {
	Normal = 0,
	Hovered = 1 << 0,
	Pressed = 1 << 1,
	Focused = 1 << 2,
	Disabled = 1 << 3
};

inline WidgetState operator|(WidgetState a, WidgetState b) {
	return static_cast<WidgetState>(
		static_cast<int>(a) | static_cast<int>(b)
	);
}

inline bool hasState(WidgetState state, WidgetState flag) {
	return (static_cast<int>(state) & static_cast<int>(flag)) != 0;
}

class Widget {
protected:
	glm::vec2 position{0, 0};
	glm::vec2 size{0, 0};

	float width = 0;
	float height = 0;

	SizeMode widthMode = SizeMode::Fixed;
	SizeMode heightMode = SizeMode::Fixed;

	Container* parent = nullptr;
	bool visible = true;
	WidgetState state = WidgetState::Normal;

	glm::vec4 margin;
	glm::vec4 padding;

	std::string id;

	void updateHoverState(const glm::vec2& mousePos);

public:
	Widget();
	virtual ~Widget() = default;

	virtual void update(float dt);
	virtual void render(Graphics::Renderer& renderer);

	virtual bool onMouseMove(const glm::vec2& pos);
	virtual bool onMouseDown(const glm::vec2& pos, Uint8 button);
	virtual bool onMouseUp(const glm::vec2& pos, Uint8 button);
	virtual bool onKeyDown(SDL_Keycode key);
	virtual bool onKeyUp(SDL_Keycode key);

	virtual void setPosition(const glm::vec2& pos);
	virtual void setSize(const glm::vec2& s);

	virtual glm::vec2 getPosition() const { return position; }
	virtual glm::vec2 getSize() const { return size; }
	virtual glm::vec2 getAbsolutePosition() const;

	virtual glm::vec2 getMinimumSize() const { return glm::vec2(0, 0); }

	void setWidth(float w, SizeMode mode = SizeMode::Fixed);
	void setHeight(float w, SizeMode mode = SizeMode::Fixed);

	void setParent(Container* p) { this->parent = p; }
	Container* getParent() const { return parent; }

	void setVisible(bool isVisible) { visible = isVisible; }
	bool isVisible() const { return visible; }

	void setEnabled(bool enabled);

	WidgetState getState() const { return state; }
	bool isEnabled() const { return !hasState(state, WidgetState::Disabled); }
	bool isHovered() const { return !hasState(state, WidgetState::Hovered); }
	bool isPressed() const { return !hasState(state, WidgetState::Pressed); }
	bool isFocused() const { return !hasState(state, WidgetState::Focused); }

	void setFocused(bool focused);
	virtual bool canFocus() const { return false; }

	bool containsPoint(const glm::vec2& point) const;

	void setMargin(float m);
	void setMargin(float top, float right, float bottom, float left);
	void setMargin(const glm::vec4& m);

	void setPadding(float p);
	void setPadding(float top, float right, float bottom, float left);
	void setPadding(const glm::vec4& p);

	const glm::vec4& getMargin() const { return margin; }
	const glm::vec4& getPadding() const { return padding; }

	void setID(const std::string& ID) { this->id = ID; }
	const std::string& getID() const { return id; }

};

} // namespace Blackthorn::UI