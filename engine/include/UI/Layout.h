#pragma once

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

namespace Blackthorn::UI {

class Container;
class Widget;

struct Alignment {

	enum class Horizontal {
		Left,
		Center,
		Right,
		Stretch
	};

	enum class Vertical {
		Top,
		Center,
		Bottom,
		Stretch
	};

	Horizontal horizontal = Horizontal::Left;
	Vertical vertical = Vertical::Top;

	static Alignment topLeft() { return {Horizontal::Left, Vertical::Top}; }
	static Alignment topCenter() { return {Horizontal::Center, Vertical::Top}; }
	static Alignment topRight() { return {Horizontal::Right, Vertical::Top}; }

	static Alignment centerLeft() { return {Horizontal::Left, Vertical::Center}; }
	static Alignment center() { return {Horizontal::Center, Vertical::Center}; }
	static Alignment centerRight() { return {Horizontal::Right, Vertical::Center}; }

	static Alignment bottomLeft() { return {Horizontal::Left, Vertical::Bottom}; }
	static Alignment bottomCenter() { return {Horizontal::Center, Vertical::Bottom}; }
	static Alignment bottomRight() { return {Horizontal::Right, Vertical::Bottom}; }

	static Alignment stretch() { return {Horizontal::Stretch, Vertical::Stretch}; }
};

class Layout {
public:
	virtual ~Layout() = default;

	virtual void apply(Container* container) = 0;
	virtual glm::vec2 calculateMinimumSize(Container* container) = 0;

	void setSpacing(float s) { spacing = s; }
	float getSpacing() const { return spacing; }

protected:
	float spacing = 0.0f;

	glm::vec2 getWidgetSize(Widget* widget, const glm::vec2& availableSpace);

	void applyAlignment(Widget* widget, const glm::vec2& position, const glm::vec2& allocatedSize, const Alignment& alignment);
};

class VerticalLayout : public Layout {
public:
	VerticalLayout() = default;

	void apply(Container* container) override;
	glm::vec2 calculateMinimumSize(Container* container) override;
};

class HorizontalLayout : public Layout {
public:
	HorizontalLayout() = default;

	void apply(Container* container) override;
	glm::vec2 calculateMinimumSize(Container* container) override;
};

class GridLayout : public Layout {
public:
	GridLayout(Uint8 col);

	void apply(Container* container) override;
	glm::vec2 calculateMinimumSize(Container* container) override;

	void setColumns(Uint8 col) { columns = col; }
	Uint8 getColumns() const { return columns; }

private:
	Uint8 columns = 1;
};

} // namespace Blackthorn::UI