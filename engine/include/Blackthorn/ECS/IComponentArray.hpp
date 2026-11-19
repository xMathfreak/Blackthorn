#pragma once

#include <vector>

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/ECS/Entity.hpp"

namespace Blackthorn::ECS {

class BLACKTHORN_API IComponentArray {
public:
	virtual ~IComponentArray() = default;
	virtual void remove(Entity entity) = 0;
	[[nodiscard]] virtual bool has(Entity entity) const = 0;
	[[nodiscard]] virtual size_t size() const = 0;
	[[nodiscard]] virtual const std::vector<Entity>& entities() const = 0;
	virtual void* getRaw(Entity entity) = 0;
	[[nodiscard]] virtual const void* getRaw(Entity entity) const = 0;
};

} // namespace Blackthorn::ECS