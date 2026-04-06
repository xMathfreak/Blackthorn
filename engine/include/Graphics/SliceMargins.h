#pragma once

namespace Blackthorn::Graphics {

struct SliceMargins {
	float left = 0.0f;
	float right = 0.0f;
	float top = 0.0f;
	float bottom = 0.0f;

	SliceMargins() = default;

	constexpr SliceMargins(float m)
		: left(m), right(m), top(m), bottom(m)
	{}

	constexpr SliceMargins(float hor, float vert)
		: left(hor), right(hor), top(vert), bottom(vert)
	{}

	constexpr SliceMargins(float l, float r, float t, float b)
		: left(l), right(r), top(t), bottom(b)
	{}

	constexpr bool isEmpty() const {
		return left == 0
			&& right == 0
			&& top == 0
			&& bottom == 0;
	}

	constexpr bool operator==(const SliceMargins& other) const {
		return left == other.left
			&& right == other.right
			&& top == other.top
			&& bottom == other.bottom;
	}

	constexpr bool operator!=(const SliceMargins& other) const {
		return !(*this == other);
	}

	static constexpr SliceMargins uniform(float size) { return SliceMargins{size}; };
};

} // namespace Blackthorn::Graphics