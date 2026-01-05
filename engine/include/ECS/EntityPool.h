#pragma once

#include <array>
#include <memory>

#include "Core/Export.h"
#include "ECS/ComponentArray.h"
#include "ECS/Detail.h"

namespace Blackthorn::ECS {

namespace Detail {
template <typename ...Components>
class View; 
} // namespace Detail

class BLACKTHORN_API EntityPool {
private:
	struct EntityData {
		Uint8 generation = 0;
		Uint64 componentMask = 0;
	};

	std::vector<EntityData> entities;
	std::vector<Uint32> freeList;
	std::array<std::unique_ptr<IComponentArray>, Detail::MAX_COMPONENTS> componentArrays;
	size_t entityCount = 0;

public:
	explicit EntityPool(size_t maxEntities = Detail::MAX_ENTITIES) {
		entities.resize(maxEntities);
		freeList.reserve(maxEntities);

		for (Sint32 i = static_cast<Sint32>(maxEntities) - 1; i >= 0; --i)
			freeList.push_back(static_cast<Uint32>(i));
	}

	Entity create() {
		if (freeList.empty()) {
			throw std::runtime_error("EntityPool: Out of entity slots");
		}

		Uint32 index = freeList.back();
		freeList.pop_back();

		Entity entity = Detail::makeEntity(index, entities[index].generation);
		++entityCount;
		return entity;
	}

	void destroy(Entity entity) {
		Uint32 index = Detail::entityIndex(entity);

		if (!isValid(entity))
			return;

		Uint64 mask = entities[index].componentMask;
		for (size_t i = 0; mask && i < Detail::MAX_COMPONENTS; ++i) {
			if (mask & (1ULL << i)) {
				if (componentArrays[i])
					componentArrays[i]->remove(entity);

				mask &= ~(1ULL << i);
			}
		}

		entities[index].componentMask = 0;
		entities[index].generation++;
		freeList.push_back(index);
		--entityCount;
	}

	bool isValid(Entity entity) const {
		if (entity == INVALID_ENTITY)
			return false;

		Uint32 index = Detail::entityIndex(entity);

		if (index >= entities.size())
			return false;

		return entities[index].generation == Detail::entityGeneration(entity);
	}

	size_t aliveCount() const { return entityCount; }

	void clear() {
		for (auto& ca : componentArrays) {
			if (ca)
				ca.reset();
		}

		freeList.clear();

		for (Sint32 i = static_cast<Sint32>(entities.size()) - 1; i >= 0; --i) {
			entities[i] = EntityData{};
			freeList.push_back(static_cast<Uint32>(i));
		}

		entityCount = 0;
	}

	const std::vector<EntityData> getEntities() { return entities; }

	template <typename Component, typename... Args>
	Component& addComponent(Entity entity, Args&&... args) {
		if (!isValid(entity))
			throw std::runtime_error("EntityPool: Invalid entity");

		size_t id = Detail::componentID<Component>();

		if (!componentArrays[id])
			componentArrays[id] = std::make_unique<ComponentArray<Component>>();

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		Component& component = array->insert(entity, std::forward<Args>(args)...);

		Uint32 index = Detail::entityIndex(entity);
		entities[index].componentMask |= Detail::componentMask<Component>();

		return component;
	}

	template <typename Component>
	void removeComponent(Entity entity) {
		if (!isValid(entity))
			return;

		size_t id = Detail::componentID<Component>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return;

		componentArrays[id]->remove(entity);

		Uint32 index = Detail::entityIndex(entity);
		entities[index].componentMask &= ~Detail::componentMask<Component>();
	}

	template <typename Component>
	bool hasComponent(Entity entity) const {
		if (!isValid(entity))
			return false;

		size_t id = Detail::componentID<Component>();

		if (id >= componentArrays.size() || !componentArrays[id])
			return false;

		return componentArrays[id]->has(entity);
	}

	template <typename Component>
	Component* getComponent(Entity entity) {
		if (!isValid(entity))
			return nullptr;

		size_t id = Detail::componentID<Component>();
		if(id >= componentArrays.size() || !componentArrays[id])
			return nullptr;

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		return array->get(entity);
	}

	template <typename Component>
	const Component* getComponent(Entity entity) const {
		if (!isValid(entity))
			return nullptr;

		size_t id = Detail::componentID<Component>();
		if(id >= componentArrays.size() || !componentArrays[id])
			return nullptr;

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		return array->get(entity);
	}

	template <typename... Components>
	Detail::View<Components...> view() {
		constexpr size_t N = sizeof...(Components);

		if constexpr (N == 0) {
			static std::vector<Entity> empty;
			return View<Components...>(this, 0, &empty);
		}

		Uint64 requiredMask = 0;
		const std::vector<Entity>* smallestList = nullptr;
		size_t smallestSize = SIZE_MAX;

		auto processComponent = [&]<typename T>() {
			using Raw = Detail::RawType<T>;
			size_t id = Detail::componentID<Raw>();

			if constexpr (!std::is_pointer_v<T>) {
				requiredMask |= Detail::componentMask<Raw>();

				if (id < componentArrays.size() && componentArrays[id]) {
					size_t size = componentArrays[id]->size();
					if (size < smallestSize) {
						smallestSize = size;
						smallestList = &componentArrays[id]->entities();
					}
				}
			}
		};

		(processComponent.template operator()<Components>(), ...);

		if (!smallestList) {
			static std::vector<Entity> empty;
			return Detail::View<Components...>(this, requiredMask, &empty);
		}

		return Detail::View<Components...>(this, requiredMask, smallestList);
	}
};

namespace Detail {

template <typename... Components>
class View {
private: 
	EntityPool* pool;
	Uint64 requiredMask;
	const std::vector<Entity>* entityList;

public:
	View(EntityPool* p, Uint64 mask, const std::vector<Entity>* entities)
		: pool(p)
		, requiredMask(mask)
		, entityList(entities)
	{}

	template <typename Function>
	void each(Function&& callback) {
		if (!entityList)
			return;

		for (Entity e : *entityList) {
			Uint32 index = Detail::entityIndex(e);

			if ((pool->getEntities()[index].componentMask & requiredMask) != requiredMask)
				continue;

			callback(e, getComponentForView<Components>(e)...);
		}
	}

private:
	template <typename Component>
	decltype(auto) getComponentForView(Entity entity) {
		using Raw = Detail::RawType<Component>;

		if constexpr (std::is_pointer_v<Component>) {
			return pool->getComponent<Raw>(entity);
		} else {
			Raw* comp = pool->getComponent<Raw>(entity);
			assert(comp != nullptr);
			return (*comp);
		}
	}
};

} // namespace Blackthorn::ECS::Detail

} // namespace Blackthorn::ECS
