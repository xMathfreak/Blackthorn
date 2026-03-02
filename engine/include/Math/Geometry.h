#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::Math::Geometry {

struct BLACKTHORN_API AABB {
	glm::vec2 min;
	glm::vec2 max;

	[[nodiscard]]
	constexpr glm::vec2 center() const {
		return (min + max) * 0.5f;
	}

	[[nodiscard]]
	constexpr glm::vec2 size() const {
		return max - min;
	}

	[[nodiscard]]
	constexpr glm::vec2 halfExtents() const {
		return size() * 0.5f;
	}

	[[nodiscard]]
	constexpr float area() const {
		glm::vec2 s = size();
		return (s.x * s.y);
	}

	[[nodiscard]]
	constexpr float perimeter() const {
		glm::vec2 s = size();
		return 2.0f * (s.x + s.y);
	}

	[[nodiscard]]
	constexpr AABB expanded(float amount) const {
		return { min - glm::vec2{amount}, max + glm::vec2{amount} };
	}

	[[nodiscard]]
	constexpr bool contains(const glm::vec2& point) const {
		return point.x >= min.x && point.x <= max.x
			&& point.y >= min.y && point.y <= max.y;
	}

	[[nodiscard]]
	constexpr bool overlaps(const AABB& other) const {
		return !(
			max.x < other.min.x ||
			min.x > other.max.x ||
			max.y < other.min.y ||
			min.y > other.max.y
		);
	}

	[[nodiscard]]
	constexpr bool intersection(const AABB& other, AABB& result) const {
		AABB r {
			glm::vec2{ std::max(min.x, other.min.x), std::max(min.y, other.min.y) },
			glm::vec2{ std::min(max.x, other.max.x), std::min(max.y, other.max.y) }
		};

		if (r.min.x >= r.max.x || r.min.y >= r.max.y)
			return false;

		result = r;
		return true;
	}

	[[nodiscard]]
	constexpr AABB merge(const AABB& other) const {
		return {
			glm::vec2{ std::min(min.x, other.min.x), std::min(min.y, other.min.y) },
			glm::vec2{ std::max(max.x, other.max.x), std::max(max.y, other.max.y) }
		};
	}

	[[nodiscard]]
	static constexpr AABB fromCenterSize(const glm::vec2& center, const glm::vec2& size) {
		glm::vec2 half = size * 0.5f;
		return { center - half, center + half };
	}
};

struct BLACKTHORN_API Circle {
	glm::vec2 center;
	float radius;

	[[nodiscard]]
	constexpr float radiusSq() const {
		return radius * radius;
	}

	[[nodiscard]]
	constexpr float area() const {
		return std::numbers::pi_v<float> * radiusSq();
	}

	[[nodiscard]]
	constexpr float circumference() const {
		return 2.0f * std::numbers::pi_v<float> * radius;
	}

	[[nodiscard]]
	constexpr bool contains(const glm::vec2& point) const {
		glm::vec2 d = point - center;
		return (d.x * d.x + d.y * d.y) <= radiusSq();
	}

	[[nodiscard]]
	constexpr bool overlaps(const Circle& other) const {
		glm::vec2 d = other.center - center;
		float radSum = radius + other.radius;
		return (d.x * d.x + d.y * d.y) <= radSum * radSum;
	}

	[[nodiscard]]
	constexpr bool overlaps(const AABB& box) const {

		glm::vec2 closest = glm::vec2{
		glm::max(box.min.x, glm::min(center.x, box.max.x)),
		glm::max(box.min.y, glm::min(center.y, box.max.y))
		};

		glm::vec2 d = center - closest;
		return (d.x * d.x + d.y * d.y) <= radiusSq();
	}

	[[nodiscard]]
	constexpr AABB bounds() const {
		return {
			center - glm::vec2{radius},
			center + glm::vec2{radius}
		};
	}
};

struct BLACKTHORN_API Ray2D {
	glm::vec2 origin;
	glm::vec2 direction;
	glm::vec2 invDirection;

	Ray2D(glm::vec2 o, glm::vec2 d)
		: origin(o)
		, direction(d)
		, invDirection(1.0f / d)
	{}
};

struct RayHit {
	glm::vec2 point;
	glm::vec2 normal;
	float distance;
};

[[nodiscard]]
constexpr bool pointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
	glm::vec2 ab = b - a, bc = c - b, ca = a - c;
	glm::vec2 ap = p - a, bp = p - b, cp = p - c;

	float d0 = ab.x * ap.y - ab.y * ap.x;
	float d1 = bc.x * bp.y - bc.y * bp.x;
	float d2 = ca.x * cp.y - ca.y * cp.x;

	bool hasNeg = (d0 < 0) || (d1 < 0) || (d2 < 0);
	bool hasPos = (d0 > 0) || (d1 > 0) || (d2 > 0);

	return !(hasNeg && hasPos);
}

[[nodiscard]]
constexpr glm::vec2 closestPointOnSegment(const glm::vec2& point, const glm::vec2& segStart, const glm::vec2& segEnd) {
	glm::vec2 ab = segEnd - segStart;
	float lenSq = ab.x * ab.x + ab.y * ab.y;

	if (lenSq == 0.0f)
		return segStart;

	float t = std::clamp(
		((point.x - segStart.x) * ab.x + (point.y - segStart.y) * ab.y) / lenSq,
		0.0f, 1.0f
	);
	return segStart + ab * t;
}

[[nodiscard]]
inline float distanceToSegment(const glm::vec2& point, const glm::vec2& segStart, const glm::vec2& segEnd) {
	glm::vec2 closest = closestPointOnSegment(point, segStart, segEnd);
	glm::vec2 d = point - closest;

	return std::sqrt(d.x * d.x + d.y * d.y);
}

[[nodiscard]]
constexpr float distanceSqToSegment(const glm::vec2& point, const glm::vec2& segStart, const glm::vec2& segEnd) {
	glm::vec2 closest = closestPointOnSegment(point, segStart, segEnd);
	glm::vec2 d = point - closest;

	return (d.x * d.x + d.y * d.y);
}

[[nodiscard]]
constexpr bool segmentIntersection(const glm::vec2& a1, const glm::vec2& a2, const glm::vec2& b1, const glm::vec2& b2, glm::vec2& outPoint) {
	glm::vec2 da = a2 - a1;
	glm::vec2 db = b2 - b1;
	float denom = da.x * db.y - da.y * db.x;

	if (std::abs(denom) < 1e-6f)
		return false;

	glm::vec2 diff = b1 - a1;

	float t = (diff.x * db.y - diff.y * db.x) / denom;
	float u = (diff.x * da.y - diff.y * da.x) / denom;

	if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f)
		return false;

	outPoint = a1 + da * t;
	return true;
}

[[nodiscard]]
inline bool raycastAABB(const Ray2D& ray, const AABB& box, RayHit& outHit) {
	glm::vec2 t0 = (box.min - ray.origin) * ray.invDirection;
	glm::vec2 t1 = (box.max - ray.origin) * ray.invDirection;

	glm::vec2 tMin = glm::min(t0, t1);
	glm::vec2 tMax = glm::max(t0, t1);

	float tEnter = std::max(tMin.x, tMin.y);
	float tExit = std::min(tMax.x, tMax.y);

	if (tEnter > tExit || tExit < 0.0f)
		return false;

	float t = tEnter >= 0.0f ? tEnter : tExit;

	outHit.distance = t;
	outHit.point = ray.origin + ray.direction * t;

	return true;
}

[[nodiscard]]
inline bool raycastCircle(const Ray2D& ray, const Circle& circle, RayHit& outHit) {
	glm::vec2 oc = ray.origin - circle.center;

	float a = glm::dot(ray.direction, ray.direction);

	if (a == 0.0f)
		return false;

	float b = 2.0f * glm::dot(oc, ray.direction);
	float c = glm::dot(oc, oc) - circle.radiusSq();

	float discriminant = b * b - 4.0f * a * c;

	if (discriminant < 0.0f)
		return false;

	float sqrtD = std::sqrt(discriminant);
	float inv2A = 1.0f / (2.0f * a);

	float t = (-b - sqrtD) * inv2A;

	if (t < 0.0f)
		t = (-b + sqrtD) * inv2A;

	if (t < 0.0f)
		return false;

	outHit.distance = t;
	outHit.point = ray.origin + ray.direction * t;
	outHit.normal = (outHit.point - circle.center) / circle.radius;

	return true;
}

[[nodiscard]]
constexpr bool aabbMinTransVec(const AABB& a, const AABB& b, glm::vec2& outVec) {
	if (!a.overlaps(b))
		return false;

	glm::vec2 aCenter = a.center();
	glm::vec2 bCenter = b.center();

	glm::vec2 aHalf = a.halfExtents();
	glm::vec2 bHalf = b.halfExtents();

	float overlapX = (aHalf.x + bHalf.x) - std::abs(aCenter.x - bCenter.x);
	float overlapY = (aHalf.y + bHalf.y) - std::abs(aCenter.y - bCenter.y);

	float sign;
	glm::vec2 res;

	if (overlapX < overlapY) {
		sign = aCenter.x < bCenter.x ? -1.0f : 1.0f;
		res = glm::vec2{ overlapX * sign, 0.0f };
	} else {
		sign = aCenter.y < bCenter.y ? -1.0f : 1.0f;
		res = glm::vec2{ 0.0f, overlapY * sign };
	}

	outVec = res;
	return true;
}

[[nodiscard]]
constexpr float triangleArea(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
	return std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
}

[[nodiscard]]
inline float angleBetween(const glm::vec2& a, const glm::vec2& b) {
	return std::atan2(b.y - a.y, b.x - a.x);
}

[[nodiscard]]
constexpr float cross2D(const glm::vec2& a, const glm::vec2& b) {
	return a.x * b.y - a.y * b.x;
}

[[nodiscard]]
inline glm::vec2 rotateAround(const glm::vec2& point, const glm::vec2& pivot, float angleRad) {
	float cosA = std::cos(angleRad);
	float sinA = std::sin(angleRad);

	glm::vec2 d = point - pivot;
	return pivot + glm::vec2{
		d.x * cosA - d.y * sinA,
		d.x * sinA + d.y * cosA
	};
}

[[nodiscard]]
constexpr bool intersectsFast(const AABB& a, const AABB& b) {
	return
		(a.min.x < b.max.x) &
		(a.max.x > b.min.x) &
		(a.min.y < b.max.y) &
		(a.max.y > b.min.y);
}

} // namespace Blackthorn::Math::Geometry