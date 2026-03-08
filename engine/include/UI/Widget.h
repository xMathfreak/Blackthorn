#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"
#include "UI/WidgetState.h"
#include "UI/Alignment.h"

namespace Blackthorn {

namespace Graphics {
	class Renderer;
} // namespace Graphics

namespace UI {

class UIManager;
class Container;

enum class SizeMode : Uint8 {
	Fixed,
	Content,
	Percent
};

class BLACKTHORN_API Widget {
	friend class UIManager;
	friend class Container;

public:
	struct Margins {
		float top = 0, right = 0, bottom = 0, left = 0;

		bool operator==(const Margins& other) {
			return top == other.top
				&& right == other.right
				&& bottom == other.bottom
				&&left == other.left;
		}
	};

protected:
	glm::vec2 position{0};
	glm::vec2 size{0};

	float designWidth = 0;
	float designHeight = 0;

	SizeMode widthMode = SizeMode::Fixed;
	SizeMode heightMode = SizeMode::Fixed;

	mutable glm::vec2 absolutePosition{0};

	mutable bool transformDirty = true;
	mutable bool layoutDirty = true;
	mutable bool renderDirty = true;

	bool visible = true;
	Container* parent = nullptr;

	WidgetState state = WidgetState::Normal;
	Alignment alignment = Alignment::topLeft();

	Margins margin;
	Margins padding;

	virtual void updateLayout();
	virtual void updateTransform() const;

	void setState(WidgetState flag);
	void clearState(WidgetState flag);
	void toggleState(WidgetState flag);

public:
	Widget();
	virtual ~Widget() = default;

	Widget(const Widget&) = delete;
	Widget& operator=(const Widget&) = delete;

	Widget(Widget&&) noexcept = default;
	Widget& operator=(Widget&&) noexcept = default;

	virtual void render(Graphics::Renderer& renderer) {}
	virtual void update(float dt) {}

	virtual void setPosition(const glm::vec2& pos);
	virtual glm::vec2 getPosition() const { return position; }
	virtual glm::vec2 getAbsolutePosition() const;

	virtual void setSize(const glm::vec2& sz);
	virtual glm::vec2 getSize() const { return size; }
	virtual glm::vec2 getMinimumSize() const { return {0, 0}; }

	void setWidth(float w, SizeMode mode = SizeMode::Fixed);
	void setHeight(float h, SizeMode mode = SizeMode::Fixed);

	SizeMode getWidthMode() const { return widthMode; }
	SizeMode getHeightMode() const { return heightMode; }

	float getDesignWidth() const { return designWidth; }
	float getDesignHeight() const { return designHeight; }

	void setAlignment(const Alignment& align);
	const Alignment& getAlignment() const { return alignment; }

	void setMargin(float m);
	void setMargin(float top, float right, float bottom, float left);
	void setPadding(float p);
	void setPadding(float top, float right, float bottom, float left);

	const Margins& getMargin() const { return margin; }
	const Margins& getPadding() const { return padding; }

	void setParent(Container* container);
	Container* getParent() const { return parent; }

	void setVisible(bool isVisible);
	bool isVisible() const { return visible; }

	void setEnabled(bool enabled);
	void setHovered(bool hovered);
	void setFocused(bool focused);
	virtual bool canFocus() const { return false; }

	WidgetState getState() const { return state; }
	bool isEnabled() const { return !hasState(state, WidgetState::Disabled); }
	bool isHovered() const { return hasState(state, WidgetState::Hovered); }
	bool isPressed() const { return hasState(state, WidgetState::Pressed); }
	bool isFocused() const { return hasState(state, WidgetState::Focused); }

	virtual bool onMouseMove(const glm::vec2& pos);
	virtual bool onMouseDown(const glm::vec2& pos, Uint8 button);
	virtual bool onMouseUp(const glm::vec2& pos, Uint8 button);
	virtual bool onKeyDown(SDL_Keycode key) { return false; }
	virtual bool onKeyUp(SDL_Keycode key) { return false; }

	bool containsPoint(const glm::vec2& point) const;

	virtual void markTransformDirty();
	virtual void markLayoutDirty();
	virtual void markRenderDirty();

	bool isTransformDirty() const { return transformDirty; }
	bool isLayoutDirty() const { return layoutDirty; }
	bool isRenderDirty() const { return renderDirty; }
};

} // namespace UI

} // namespace Blackthorn