#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace Blackthorn::Math {

template <typename T = float>
requires std::is_arithmetic_v<T>
struct Edges {
	T left{0};
	T right{0};
	T top{0};
	T bottom{0};

	constexpr Edges() = default;

	constexpr Edges(T scalar)
		: left(scalar), right(scalar), top(scalar), bottom(scalar)
	{}

	constexpr Edges(T hor, T vert)
		: left(hor), right(hor), top(vert), bottom(vert)
	{}

	constexpr Edges(T l, T r, T t, T b)
		: left(l), right(r), top(t), bottom(b)
	{}

	static constexpr Edges uniform(T scalar) {
		return Edges{scalar};
	}

	static constexpr Edges horizontal(T v) {
		return Edges{v, v, T{0}, T{0}};
	}

	static constexpr Edges vertical(T v) {
		return Edges{T{0}, T{0}, v, v};
	}

	constexpr bool isZero() const {
		return left == T{0}
			&& right == T{0}
			&& top == T{0}
			&& bottom == T{0};
	}

	constexpr bool hasAny() const {
		return !isZero();
	}

	constexpr bool operator==(const Edges&) const = default;

	constexpr Edges operator+(const Edges& o) const {
		return Edges{
			this->left + o.left,
			this->right + o.right,
			this->top + o.top,
			this->bottom + o.bottom
		};
	}

	constexpr Edges operator-() const {
		return Edges{-left, -right, -top, -bottom};
	}

	constexpr Edges operator*(T scalar) const {
		return Edges{
			this->left * scalar,
			this->right * scalar,
			this->top * scalar,
			this->bottom * scalar
		};
	}

	constexpr Edges& operator+=(const Edges& o) {
		left += o.left;
		right += o.right;
		top += o.top;
		bottom += o.bottom;
		return *this;
	}

	constexpr Edges& operator*=(T scalar) {
		left *= scalar;
		right *= scalar;
		top *= scalar;
		bottom *= scalar;

		return *this;
	}

	friend constexpr Edges operator*(T scalar, const Edges& e) {
		return e * scalar;
	}

	constexpr Edges operator-(const Edges& o) const {
		return Edges{
			left - o.left,
			right - o.right,
			top - o.top,
			bottom - o.bottom
		};
	}

	constexpr Edges& operator-=(const Edges& o) {
		left -= o.left;
		right -= o.right;
		top -= o.top;
		bottom -= o.bottom;

		return *this;
	}

	constexpr T horizontal() const { return left + right; }
	constexpr T vertical() const { return top + bottom; }

	constexpr T max() const {
		return std::max(std::max(left, right), std::max(top, bottom));
	}

	constexpr T min() const {
		return std::min(std::min(left, right), std::min(top, bottom));
	}

	constexpr Edges abs() const {
		using std::abs;

		return Edges{
			abs(left),
			abs(right),
			abs(top),
			abs(bottom)
		};
	}

	constexpr Edges horizontalOnly() const {
		return Edges{left, right, T{0}, T{0}};
	}

	constexpr Edges verticalOnly() const {
		return Edges{T{0}, T{0}, top, bottom};
	}

	constexpr Edges clamp(const Edges& minE, const Edges& maxE) const {
		return Edges{
			std::clamp(left, minE.left, maxE.left),
			std::clamp(right, minE.right, maxE.right),
			std::clamp(top, minE.top, maxE.top),
			std::clamp(bottom, minE.bottom, maxE.bottom)
		};
	}
};

} // namespace Blackthorn::Math