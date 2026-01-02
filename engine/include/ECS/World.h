#pragma once

#include "Core/Export.h"
#include "ECS/EntityPool.h"
#include "ECS/SystemManager.h"

namespace Blackthorn::ECS {

class BLACKTHORN_API World {
public:
	World(size_t maxEntities = Detail::MAX_ENTITIES)
		: pool(maxEntities)
		, systemManager(pool)
	{}

	~World() {}

	Entity createEntity() {
		return pool.create();
	}

	void destroyEntity(Entity entity) {
		pool.destroy(entity);
	}

	bool isValid(Entity entity) const {
		return pool.isValid(entity);
	}

	size_t getEntityCount() const {
		return pool.aliveCount();
	}

	void clear() {
		pool.clear();
	}

	template <typename Component, typename... Args>
	Component& addComponent(Entity entity, Args&&... args) {
		pool.addComponent<Component>(entity, std::forward<Args>(args)...);
	}

	template <typename Component>
	void removeComponent(Entity entity) {
		pool.removeComponent<Component>(entity);
	}

	template <typename Component>
	bool hasComponent(Entity entity) const {
		return pool.hasComponent<Component>(entity);
	}

	template <typename Component>
	Component* getComponent(Entity entity) {
		return pool.getComponent<Component>(entity);
	}

	template <typename Component>
	const Component* getComponent(Entity entity) const {
		return pool.getComponent<Component>(entity);
	}

	template <typename... Components>
	Detail::View<Components...> view() {
		return pool.view<Components...>();
	}


	template <typename System, typename... Args>
	System* addSystem(Args&&... args) {
		return systemManager.add<System>(std::forward<Args>(args)...);
	}

	template <typename System>
	System* getSystem() {
		return systemManager.get<System>();
	}

	template <typename System>
	void removeSystem() {
		systemManager.remove<System>();
	}

	void update(float dt) {
		systemManager.update(dt);
	}

	void fixedUpdate(float dt) {
		systemManager.fixedUpdate(dt);
	}

	void render(float alpha) {
		systemManager.render(alpha);
	}

private:
	EntityPool pool;
	Systems::SystemManager systemManager;
};

} // namespace Blackthorn::ECS