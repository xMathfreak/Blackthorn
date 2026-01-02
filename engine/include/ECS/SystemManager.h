#pragma once

#include "Core/Export.h"
#include "ECS/ISystem.h"

#include <vector>

namespace Blackthorn::ECS::Systems {

class BLACKTHORN_API SystemManager {
private:
	EntityPool& pool;
	std::vector<std::unique_ptr<ISystem>> systems;

public:
	explicit SystemManager(EntityPool& p)
		: pool(p)
	{}

	template <typename System, typename... Args>
	System* add(Args&&... args) {
		static_assert(std::is_base_of_v<ISystem, System>, "System must inherit from ISystem");
		
		auto system = std::make_unique<System>(std::forward<Args>(args)...);
		System* ptr = system.get();
		ptr->init(&pool);
		systems.push_back(std::move(system));
		return ptr;
	}
};

} // namespace Blackthorn::ECS::Systems