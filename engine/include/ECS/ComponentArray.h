#pragma once

#include "ECS/Detail.h"
#include "ECS/IComponentArray.h"

namespace Blackthorn::ECS {

template <typename T>
class ComponentArray : public IComponentArray {
private:
	struct SparseEntry {
		Uint32 pos;
		Uint8 generation;
	};

private:
	std::vector<T> components;
	std::vector<Entity> dense;
	std::vector<SparseEntry> sparse;

public:
	explicit ComponentArray(size_t reserve = Detail::MAX_ENTITIES) {
		components.reserve(reserve);
		dense.reserve(reserve);
		sparse.assign(reserve, { INVALID_ENTITY, 0 });
	}

	template <typename... Args>
	T& insert(Entity entity, Args&&... args) {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 gen = Detail::entityGeneration(entity);

		auto& entry = sparse[idx];

		#ifdef BLACKTHORN_DEBUG
			assert(idx < sparse.size());
		#endif

		if (entry.pos != INVALID_ENTITY && entry.generation == gen) {
			components[entry.pos] = T{ std::forward<Args>(args)... };
			return components[entry.pos];
		}

		entry.pos = static_cast<Uint32>(components.size());
		entry.generation = gen;

		components.emplace_back(std::forward<Args>(args)...);
		dense.push_back(entity);

		return components.back();
	}

	void remove(Entity entity) override {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 gen = Detail::entityGeneration(entity);

		auto& entry = sparse[idx];

		if (entry.pos == INVALID_ENTITY || entry.generation != gen)
			return;

		Uint32 pos = entry.pos;
		Uint32 lastPos = static_cast<Uint32>(components.size() - 1);

		if (pos != lastPos) {
			components[pos] = std::move(components[lastPos]);
			Entity moved = dense[lastPos];
			dense[pos] = moved;

			auto& movedEntry = sparse[Detail::entityIndex(moved)];
			movedEntry.pos = pos;
		}

		components.pop_back();
		dense.pop_back();
		entry.pos = INVALID_ENTITY;
	}

	bool has(Entity entity) const override {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 gen = Detail::entityGeneration(entity);

		const auto& entry = sparse[idx];

		return entry.pos != INVALID_ENTITY && entry.generation == gen;
	}

	T* get(Entity entity) {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 gen = Detail::entityGeneration(entity);

		auto& entry = sparse[idx];
		return (entry.pos != INVALID_ENTITY && entry.generation == gen)
			? &components[entry.pos]
			: nullptr;
	}

	const T* get(Entity entity) const {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 gen = Detail::entityGeneration(entity);

		auto& entry = sparse[idx];
		return (entry.pos != INVALID_ENTITY && entry.generation == gen)
			? &components[entry.pos]
			: nullptr;
	}

	size_t size() const override { return components.size(); }
	const std::vector<Entity>& entities() const override { return dense; }

	T& getByIndex(size_t i) { return components[i]; }
	const T& getByIndex(size_t i) const { return components[i]; }

	T* data() noexcept { return components.data(); }
	const T* data() const noexcept { return components.data(); }
};

} // namespace Blackthorn::ECS
