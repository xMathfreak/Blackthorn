#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::UI {

enum class WidgetState : U8 {
	Normal = 0,
	Hovered = 1 << 0,
	Pressed = 1 << 1,
	Focused = 1 << 2,
	Disabled = 1 << 3
};

inline WidgetState operator|(WidgetState a, WidgetState b) {
	return static_cast<WidgetState>(
		static_cast<U8>(a) | static_cast<U8>(b)
	);
}

inline WidgetState operator&(WidgetState a, WidgetState b) {
	return static_cast<WidgetState>(
		static_cast<U8>(a) & static_cast<U8>(b)
	);
}

inline WidgetState operator^(WidgetState a, WidgetState b) {
	return static_cast<WidgetState>(
		static_cast<U8>(a) ^ static_cast<U8>(b)
	);
}

inline WidgetState operator~(WidgetState a) {
	return static_cast<WidgetState>(
		~static_cast<U8>(a)
	);
}

inline WidgetState& operator|=(WidgetState& a, WidgetState b) {
	a = a | b;
	return a;
}

inline WidgetState& operator&=(WidgetState& a, WidgetState b) {
	a = a & b;
	return a;
}

inline bool hasState(WidgetState state, WidgetState flag) {
	return (static_cast<U8>(state) & static_cast<U8>(flag)) != 0;
}

} // namespace Blackthorn::UI