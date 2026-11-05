#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <vector>

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
		U16 generation = 0;
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
		if (maxEntities > (U32{1} << Detail::INDEX_BITS))
			throw std::invalid_argument("EntityPool: maxEntities exceeds the entity index range (" + std::to_string(Detail::INDEX_MASK) + ")");

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
		--entityCount;

		// If the generation wrapped to zero, don't reuse the slot. A handle
		// from 65536 reuses ago would otherwise match a live entity again.
		if (entities[index].generation != 0)
			freeList.push_back(index);

		bumpEpoch();
	}

	bool isValid(Entity entity) const {
		if (entity == INVALID_ENTITY)
			return false;

		const U32 index = Detail::entityIndex(entity);
		if (index >= entities.size())
			return false;

		const EntityData& ed = entities[index];
		return ed.alive && ed.generation == Detail::entityGeneration(entity);
	}

	size_t aliveCount() const { return entityCount; }

	void clear() {
		for (auto& ca : componentArrays)
			ca.reset();

		freeList.clear();

		for (I32 i = static_cast<I32>(entities.size()) - 1; i >= 0; --i) {
			// Keep the generation and advance it, so every existing handle goes stale.
			const U16 next = static_cast<U16>(entities[i].generation + 1);
			entities[i] = EntityData{};
			entities[i].generation = next;

			if (next != 0)
				freeList.push_back(static_cast<U32>(i));
		}

		entityCount = 0;
		bumpEpoch();
	}

	/**
	 * @brief Returns the first live entity whose component C satisfies @p pred.
	 *
	 * Scans only the entities that have C. Order is unspecified, so th
	 * predicate should match at most one entity for a unique key.
	 */
	template <typename C, typename Pred>
	std::optional<Entity> findEntity(Pred&& pred) const {
		const size_t id = Detail::componentID<C>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return std::nullopt;

		const auto* array = static_cast<const ComponentArray<C>*>(componentArrays[id].get());
		for (const Entity entity : componentArrays[id]->entities()) {
			const C* component = array->get(entity);
			if (component && pred(*component))
				return entity;
		}

		return std::nullopt;
	}

	/**
	 * @brief Returns every entity whose component C satisfies @p pred.
	 *
	 * Scans only entities that have C. Result order is unspecified.
	 */
	template <typename C, typename Pred>
	std::vector<Entity> collectEntities(Pred&& pred) const {
		std::vector<Entity> result;

		const size_t id = Detail::componentID<C>();
		if (id >= componentArrays.size() || !componentArrays[id])
			return result;

		const auto* array = static_cast<const ComponentArray<C>*>(componentArrays[id].get());
		for (const Entity entity : componentArrays[id]->entities()) {
			const C* component = array->get(entity);
			if (component && pred(*component))
				result.push_back(entity);
		}

		return result;
	}

	std::span<const EntityData> getEntities() const { return entities; }

	U64 getEpoch() const noexcept {
		return structuralEpoch.load(std::memory_order::acquire);
	}

	template <typename Component, typename... Args>
	Component& addComponent(Entity entity, Args&&... args) {
		if (!isValid(entity))
			throw std::runtime_error("EntityPool: Invalid entity");

		static const size_t id = Detail::componentID<Component>();

		if (!componentArrays[id])
			componentArrays[id] = std::make_unique<ComponentArray<Component>>(entities.size());

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
	mutable std::atomic<U64> cachedEpoch{ U64_MAX };

public:
	View(EntityPool* p, U64 mask, const std::vector<Entity>* entities)
		: pool(p)
		, requiredMask(mask)
		, entityList(entities)
	{}

	View(const View&) = delete;
	View& operator=(const View&) = delete;
	View& operator=(View&&) = delete;

	View(View&& other) noexcept
		: pool(other.pool)
		, requiredMask(other.requiredMask)
		, entityList(other.entityList)
		, cachedMatching(std::move(other.cachedMatching))
		, cachedEpoch(other.cachedEpoch.load(std::memory_order::acquire))
	{
		other.cachedEpoch.store(U64_MAX, std::memory_order::release);
	}

	/**
	 * @brief Calls @p callback for every entity with all required components.
	 *        Runs on the calling thread.
	 *
	 * @param callback Invocable as `(Entity, Components&...)`, or
	 *                 `(Entity, Components*...)` for pointer types.
	 */
	template <typename Callable>
	void each(Callable&& callback) {
		const std::shared_lock<std::shared_mutex> lock = acquireCache();

		for (Entity e : cachedMatching)
			callback(e, getComponentForView<Components>(e)...);
	}

	/**
	 * @brief Dispatches @p callback across worker threads, then blocks until
	 *        every batch has finished.
	 *
	 * Falls back to serial iteration when @p js is null or the match count is
	 * below @p threshold. The callback must be safe to call concurrently on
	 * different entities and must not add or remove components or entities.
	 *
	 * @param threshold Minimum matches before parallel dispatch. A value of 0
	 *                  is treated as 1.
	 */
	template <typename Callable>
	void eachJobs(Jobs::JobSystem* js, Callable&& callback, size_t threshold = 64) {
		if (threshold == 0)
			threshold = 1;

		const std::shared_lock<std::shared_mutex> lock = acquireCache();
		const std::vector<Entity>& entities = cachedMatching;
		const size_t count = entities.size();

		if (count == 0)
			return;

		if (!js || count < threshold) {
			for (Entity e : entities)
				callback(e, getComponentForView<Components>(e)...);
			return;
		}

		const size_t batchCount = (count + threshold - 1) / threshold;
		auto handle = js->createHandle();
		handle->addPending(static_cast<int>(batchCount) - 1);

		for (size_t b = 0; b < batchCount; ++b) {
			const size_t begin = b * threshold;
			const size_t end = std::min(begin + threshold, count);

			js->submit(Jobs::Job(
				[this, &entities, &callback, begin, end]() {
					for (size_t i = begin; i < end; ++i)
						callback(entities[i], getComponentForView<Components>(entities[i])...);
				},
				handle
			));
		}

		js->wait(handle);
	}

private:
	/**
	 * @brief Returns a shared lock on a cache that matches the pool's current
	 *        epoch, rebuilding it first if needed.
	 *
	 * The fast path takes only a shared lock. A stale cache is rebuilt under an
	 * exclusive lock, which rechecks the epoch so only one thread rebuilds.
	 *
	 * Structural changes to the pool must not run while iteration is in
	 * progress. This lock protects the cache, not the pool.
	 */
	std::shared_lock<std::shared_mutex> acquireCache() const {
		{
			std::shared_lock<std::shared_mutex> lock(cacheMutex);
			if (cachedEpoch.load(std::memory_order::acquire) == pool->getEpoch())
				return lock;
		}

		{
			std::unique_lock<std::shared_mutex> lock(cacheMutex);
			const U64 epoch = pool->getEpoch();
			if (cachedEpoch.load(std::memory_order::acquire) != epoch)
				rebuildLocked(epoch);
		}

		return std::shared_lock<std::shared_mutex>(cacheMutex);
	}

	/// Rebuilds cachedMatching. The caller must hold cacheMutex exclusively.
	void rebuildLocked(U64 epoch) const {
		cachedMatching.clear();
		cachedMatching.reserve(entityList->size());

		for (Entity e : *entityList) {
			if (matchesMask(e))
				cachedMatching.push_back(e);
		}

		cachedEpoch.store(epoch, std::memory_order::release);
	}

	bool matchesMask(Entity e) const {
		const U32 idx = Detail::entityIndex(e);
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