#pragma once

#include <algorithm>
#include <vector>

#include "Core/Export.h"
#include "ECS/ISystem.h"

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

	template <typename System>
	System* get() {
		auto it = std::find_if(
			systems.begin(),
			systems.end(),
			[](const std::unique_ptr<ISystem>& system) {
				return dynamic_cast<System*>(system.get()) != nullptr;
			}
		);

		if (it != systems.end())
			return dynamic_cast<System*>(it->get());

		return nullptr;
	}

	template <typename System>
	void remove() {
		auto it = std::remove_if(
			systems.begin(),
			systems.end(),
			[](const std::unique_ptr<ISystem>& system) {
				return dynamic_cast<System*>(system.get()) != nullptr;
			}
		);

		if (it != systems.end())
			systems.erase(it, systems.end());
	}

	void update(float dt) {
		for (auto& system: systems)
			system->update(&pool, dt);
	}

	void fixedUpdate(float dt) {
		for (auto& system: systems)
			system->fixedUpdate(&pool, dt);
	}

	void render(float alpha) {
		for (auto& system: systems)
			system->render(&pool, alpha);
	}
};

} // namespace Blackthorn::ECS::Systems