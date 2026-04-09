#pragma once

#include <concepts>

#include "Core/Export.h"
#include "ECS/EntityPool.h"

namespace Blackthorn::ECS::Systems {

class BLACKTHORN_API ISystem {
public:
	virtual ~ISystem() = default;
	virtual void init(EntityPool*) {}
	virtual void update(EntityPool*, float dt, Jobs::JobSystem*) {}
	virtual void fixedUpdate(EntityPool*, float dt, Jobs::JobSystem*) {}
	virtual void render(EntityPool*, float alpha) {}
	virtual void lateUpdate(EntityPool*, float dt, Jobs::JobSystem*) {}
};

template <typename T>
concept SystemType = std::derived_from<T, ISystem>;

} // namespace Blackthorn::ECS::Systems