#pragma once

#include "Core/Export.h"
#include "ECS/Entity.h"

#include <vector>

namespace Blackthorn::ECS {

class BLACKTHORN_API IComponentArray {
public:
	virtual ~IComponentArray() = default;
	virtual void remove(Entity entity) = 0;
	virtual bool has(Entity entity) const = 0;
	virtual size_t size() const = 0;
	virtual const std::vector<Entity>& entities() const = 0;
};

} // namespace Blackthorn::ECS