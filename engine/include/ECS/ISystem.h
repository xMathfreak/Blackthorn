#pragma once

#include "Core/Export.h"
#include "ECS/EntityPool.h"

namespace Blackthorn::ECS::Systems {

class BLACKTHORN_API ISystem {
public:
	virtual ~ISystem() = default;
	virtual void init(EntityPool*) {}
	virtual void update(EntityPool*, float dt) {}
	virtual void fixedUpdate(EntityPool*, float dt) {}
	virtual void render(EntityPool*, float alpha) {}
};

} // namespace Blackthorn::ECS::Systems