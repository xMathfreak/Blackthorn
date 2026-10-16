#pragma once

#include <algorithm>
#include <vector>

#include "Core/Export.h"
#include "ECS/ISystem.h"

namespace Blackthorn {

namespace Jobs {
	class JobSystem;
} // namespace Jobs

namespace ECS::Systems {

class BLACKTHORN_API SystemManager {
private:
	EntityPool& pool;
	Jobs::JobSystem* jobs;
	std::vector<std::unique_ptr<ISystem>> systems;

public:
	explicit SystemManager(EntityPool& p, Jobs::JobSystem* js = nullptr)
		: pool(p)
		, jobs(js)
	{}

	template <SystemType System, typename... Args>
	System* add(Args&&... args) {
		auto system = std::make_unique<System>(std::forward<Args>(args)...);
		System* ptr = system.get();
		ptr->init(&pool);
		systems.push_back(std::move(system));
		return ptr;
	}

	template <SystemType System>
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

	template <SystemType System>
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
			system->update(&pool, dt, jobs);
	}

	void fixedUpdate(float dt) {
		for (auto& system: systems)
			system->fixedUpdate(&pool, dt, jobs);
	}

	void render(float alpha) {
		for (auto& system: systems)
			system->render(&pool, alpha);
	}

	void lateUpdate(float dt) {
		for (auto& system : systems)
			system->lateUpdate(&pool, dt, jobs);
	}
};

} // namespace ECS::Systems

} // namespace Blackthorn