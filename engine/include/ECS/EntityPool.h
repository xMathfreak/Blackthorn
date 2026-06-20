#pragma once

#include <array>
#include <memory>
#include <shared_mutex>

#include "Core/Export.h"
#include "ECS/ComponentArray.h"
#include "ECS/Detail.h"
#include "Jobs/JobSystem.h"

namespace Blackthorn::ECS {

namespace Detail {

template <typename ...Components>
class View;

} // namespace Detail

class BLACKTHORN_API EntityPool {
private:
	struct EntityData {
		U64 componentMask = 0;
		U8 generation = 0;
		bool alive = false;
	};

	std::vector<EntityData> entities;
	std::vector<U32> freeList;
	std::array<std::unique_ptr<IComponentArray>, Detail::MAX_COMPONENTS> componentArrays;
	size_t entityCount = 0;

	std::atomic<U64> structuralEpoch{0};

	void bumpEpoch() noexcept {
		structuralEpoch.fetch_add(1, std::memory_order::release);
	}

public:
	explicit EntityPool(size_t maxEntities = Detail::MAX_ENTITIES) {
		entities.resize(maxEntities);
		freeList.reserve(maxEntities);

		for (I32 i = static_cast<I32>(maxEntities) - 1; i >= 0; --i)
			freeList.push_back(static_cast<U32>(i));
	}

	Entity create() {
		if (freeList.empty())
			throw std::runtime_error("EntityPool: Out of entity slots");

		U32 index = freeList.back();
		freeList.pop_back();

		#ifdef BLACKTHORN_DEBUG
			assert(index < entities.size());
		#endif

		Entity entity = Detail::makeEntity(index, entities[index].generation);
		entities[index].alive = true;
		++entityCount;

		bumpEpoch();
		return entity;
	}

	void destroy(Entity entity) {
		U32 index = Detail::entityIndex(entity);

		if (!isValid(entity))
			return;

		U64 mask = entities[index].componentMask;
		while (mask) {
			const size_t i = static_cast<size_t>(std::countr_zero(mask));
			if (componentArrays[i])
				componentArrays[i]->remove(entity);

			mask &= mask - 1;
		}

		entities[index].componentMask = 0;
		entities[index].alive = false;
		entities[index].generation++;
		freeList.push_back(index);
		--entityCount;
		bumpEpoch();
	}

	bool isValid(Entity entity) const {
		if (entity == INVALID_ENTITY)
			return false;

		U32 index = Detail::entityIndex(entity);

		if (index >= entities.size())
			return false;

		const EntityData& ed = entities[index];
		return ed.alive && entities[index].generation == Detail::entityGeneration(entity);
	}

	size_t aliveCount() const { return entityCount; }

	void clear() {
		for (auto& ca : componentArrays) {
			if (ca)
				ca.reset();
		}

		freeList.clear();

		for (I32 i = static_cast<I32>(entities.size()) - 1; i >= 0; --i) {
			entities[i] = EntityData{};
			freeList.push_back(static_cast<U32>(i));
		}

		entityCount = 0;
		bumpEpoch();
	}

	const std::vector<EntityData>& getEntities() const { return entities; }
	std::vector<EntityData>& getEntities() { return entities; }

	U64 getEpoch() const noexcept {
		return structuralEpoch.load(std::memory_order::acquire);
	}

	template <typename Component, typename... Args>
	Component& addComponent(Entity entity, Args&&... args) {
		if (!isValid(entity))
			throw std::runtime_error("EntityPool: Invalid entity");

		static const size_t id = Detail::componentID<Component>();

		if (!componentArrays[id])
			componentArrays[id] = std::make_unique<ComponentArray<Component>>();

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		Component& component = array->insert(entity, std::forward<Args>(args)...);

		U32 index = Detail::entityIndex(entity);
		entities[index].componentMask |= Detail::componentMask<Component>();

		bumpEpoch();
		return component;
	}

	template <typename Component>
	void removeComponent(Entity entity) {
		if (!isValid(entity))
			return;

		static const size_t id = Detail::componentID<Component>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return;

		componentArrays[id]->remove(entity);

		U32 index = Detail::entityIndex(entity);
		entities[index].componentMask &= ~Detail::componentMask<Component>();

		bumpEpoch();
	}

	template <typename Component>
	bool hasComponent(Entity entity) const {
		if (!isValid(entity))
			return false;

		static const size_t id = Detail::componentID<Component>();

		if (id >= componentArrays.size() || !componentArrays[id])
			return false;

		return componentArrays[id]->has(entity);
	}

	template <typename Component>
	Component* getComponent(Entity entity) {
		if (!isValid(entity))
			return nullptr;

		static const size_t id = Detail::componentID<Component>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return nullptr;

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		return array->get(entity);
	}

	template <typename Component>
	const Component* getComponent(Entity entity) const {
		if (!isValid(entity))
			return nullptr;

		static const size_t id = Detail::componentID<Component>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return nullptr;

		auto* array = static_cast<ComponentArray<Component>*>(componentArrays[id].get());
		return array->get(entity);
	}

	void* getComponentRaw(Entity entity, size_t componentIndex) {
		if (!isValid(entity))
			return nullptr;

		if (componentIndex >= componentArrays.size() || !componentArrays[componentIndex])
			return nullptr;

		if (!componentArrays[componentIndex]->has(entity))
			return nullptr;

		return componentArrays[componentIndex]->getRaw(entity);
	}

	const void* getComponentRaw(Entity entity, size_t componentIndex) const {
		return const_cast<EntityPool*>(this)->getComponentRaw(entity, componentIndex);
	}

	template <typename... Components>
	Detail::View<Components...> view() {
		constexpr size_t N = sizeof...(Components);

		if constexpr (N == 0) {
			static std::vector<Entity> empty;
			return Detail::View<Components...>(this, 0, &empty);
		}

		U64 requiredMask = 0;
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
	U64 requiredMask;
	const std::vector<Entity>* entityList;

	mutable std::shared_mutex cacheMutex;
	mutable std::vector<Entity> cachedMatching;
	mutable U64 cachedEpoch = U64_MAX;

public:
	View(EntityPool* p, U64 mask, const std::vector<Entity>* entities)
		: pool(p)
		, requiredMask(mask)
		, entityList(entities)
	{}

	View(const View&) = delete;
	View& operator=(const View&) = delete;

	View(View&& other) noexcept
		: pool(other.pool)
		, requiredMask(other.requiredMask)
		, entityList(other.entityList)
		, cachedMatching(std::move(other.cachedMatching))
		, cachedEpoch(other.cachedEpoch)
	{
		other.cachedEpoch = U64_MAX;
	}

	View& operator=(View&&) = delete;

	/**
	 * @brief Calls @p callback for every entity that has all required
	 *        components. Runs on the calling thread.
	 *
	 * @param callback Invocable as `(Entity, Components&...)` or `(Entity,
	 *                 Components*...)` for optional pointer types.
	 */
	template <typename Callable>
	void each(Callable&& callback) {
		rebuildCache();

		std::shared_lock lock(cacheMutex);
		for (Entity e : cachedMatching)
			callback(e, getComponentForView<Components>(e)...);
	}

	/**
	 * @brief Dispatches @p callback across worker threads using @p js,
	 *        then blocks until all batches have completed.
	 *
	 * The callback signature is identical to `each()`. The batching is
	 * fully transparent to the caller. Falls back to `each()` if @p js
	 * is null or the matching entity count is below @p threshold.
	 *
	 * Every batch runs to completion before this function returns , so
	 * it is safe to read or write component data immediately after the
	 * call, just as with `each()`.
	 *
	 * @param js        Job system to dispatch on. Triggers serial fallback
	 *                  when null.
	 * @param callback  Per-entity work. Must be safe to call concurrently
	 *                  on different entities. Must not add or remove
	 *                  components or entities during execution.
	 * @param threshold Minimum matching entity count before parallel
	 *                  dispatch is used. Defaults to 64.
	 */
	template <typename Callable>
	void eachJobs(Jobs::JobSystem* js, Callable&& callback, size_t threshold = 64) {
		rebuildCache();

		std::shared_lock lock(cacheMutex);
		const std::vector<Entity>& entities = cachedMatching;
		const size_t count = entities.size();

		if (count == 0)
			return;

		if (!js || count < threshold) {
			for (Entity e : entities)
				callback(e, getComponentForView<Components>(e)...);

			return;
		}

		auto handle = js->createHandle();
		handle->addPending(static_cast<int>((count + threshold - 1) / threshold) - 1);

		for (size_t b = 0; b * threshold < count; ++b) {
			const size_t begin = b * threshold;
			const size_t end = std::min(begin + threshold, count);

			js->submit(Jobs::Job(
				[&entities, &callback, begin, end, this]() {
					for (size_t i = begin; i < end; ++i)
						callback(entities[i], getComponentForView<Components>(entities[i])...);
				},
				handle
			));
		}

		js->wait(handle);
	}

private:
	void rebuildCache() const {
		if (cachedEpoch == pool->getEpoch())
			return;

		std::unique_lock lock(cacheMutex);

		if (cachedEpoch == pool->getEpoch())
			return;

		cachedMatching.clear();
		if (cachedMatching.capacity() < entityList->size())
			cachedMatching.reserve(entityList->size());

		for (Entity e : *entityList) {
			if (matchesMask(e))
				cachedMatching.push_back(e);
		}

		cachedEpoch = pool->getEpoch();
	}

	bool matchesMask(Entity e) const {
		U32 idx = Detail::entityIndex(e);
		return (pool->getEntities()[idx].componentMask & requiredMask) == requiredMask;
	}

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

} // namespace Detail

} // namespace Blackthorn::ECS