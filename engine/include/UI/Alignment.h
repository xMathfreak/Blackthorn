#pragma once

#include "Core/Types/Numeric.h"

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

	static Alignment topLeft() { return {Horizontal::Left, Vertical::Top}; }
	static Alignment topCenter() { return {Horizontal::Center, Vertical::Top}; }
	static Alignment topRight() { return {Horizontal::Right, Vertical::Top}; }

	static Alignment centerLeft() { return {Horizontal::Left, Vertical::Center}; }
	static Alignment center() { return {Horizontal::Center, Vertical::Center}; }
	static Alignment centerRight() { return {Horizontal::Right, Vertical::Center}; }

	static Alignment bottomLeft() { return {Horizontal::Left, Vertical::Bottom}; }
	static Alignment bottomCenter() { return {Horizontal::Center, Vertical::Bottom}; }
	static Alignment bottomRight() { return {Horizontal::Right, Vertical::Bottom}; }

	bool operator==(const Alignment& other) const {
		return horizontal == other.horizontal && vertical == other.vertical;
	}

	bool operator!=(const Alignment& other) const {
		return !(*this == other);
	}
};

} // namespace Blackthorn::UI