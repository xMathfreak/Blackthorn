#pragma once

#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::UI {

struct Alignment {
	enum class Horizontal : U8 {
		Left,
		Center,
		Right
	};

	enum class Vertical : U8 {
		Top,
		Center,
		Bottom
	};

	Horizontal horizontal = Horizontal::Left;
	Vertical vertical = Vertical::Top;

	static Alignment topLeft() { return {.horizontal=Horizontal::Left, .vertical=Vertical::Top}; }
	static Alignment topCenter() { return {.horizontal=Horizontal::Center, .vertical=Vertical::Top}; }
	static Alignment topRight() { return {.horizontal=Horizontal::Right, .vertical=Vertical::Top}; }

	static Alignment centerLeft() { return {.horizontal=Horizontal::Left, .vertical=Vertical::Center}; }
	static Alignment center() { return {.horizontal=Horizontal::Center, .vertical=Vertical::Center}; }
	static Alignment centerRight() { return {.horizontal=Horizontal::Right, .vertical=Vertical::Center}; }

	static Alignment bottomLeft() { return {.horizontal=Horizontal::Left, .vertical=Vertical::Bottom}; }
	static Alignment bottomCenter() { return {.horizontal=Horizontal::Center, .vertical=Vertical::Bottom}; }
	static Alignment bottomRight() { return {.horizontal=Horizontal::Right, .vertical=Vertical::Bottom}; }

	bool operator==(const Alignment& other) const {
		return horizontal == other.horizontal && vertical == other.vertical;
	}

	bool operator!=(const Alignment& other) const {
		return !(*this == other);
	}
};

} // namespace Blackthorn::UI