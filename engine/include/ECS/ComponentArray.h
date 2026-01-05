#pragma once

#include "ECS/Detail.h"
#include "ECS/IComponentArray.h"

namespace Blackthorn::ECS {

template <typename T>
class ComponentArray : public IComponentArray {
private:
	std::vector<T> components;
	std::vector<Entity> dense;
	std::vector<Uint32> sparse;

public:
	explicit ComponentArray(size_t reserve = Detail::MAX_ENTITIES) {
		components.reserve(reserve);
		dense.reserve(reserve);
		sparse.assign(reserve, INVALID_ENTITY);
	}

	template <typename... Args>
	T& insert(Entity entity, Args&&... args) {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 pos = sparse[idx];

		if (pos != INVALID_ENTITY && dense[pos] == entity) {
			components[pos] = T{ std::forward<Args>(args)... };
			return components[pos];
		}

		pos = static_cast<Uint32>(components.size());
		components.emplace_back(std::forward<Args>(args)...);
		dense.push_back(entity);
		sparse[idx] = pos;
		return components.back();
	}

	void remove(Entity entity) override {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 pos = sparse[idx];

		if (pos == INVALID_ENTITY || dense[pos] != entity)
			return;

		Uint32 lastPos = static_cast<Uint32>(components.size() - 1);
		if (pos != lastPos) {
			components[pos] = std::move(components[lastPos]);
			dense[pos] = dense[lastPos];
			sparse[Detail::entityIndex(dense[pos])] = pos;
		}

		components.pop_back();
		dense.pop_back();
		sparse[idx] = INVALID_ENTITY;
	}

	bool has(Entity entity) const override {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 pos = sparse[idx];
		return pos != INVALID_ENTITY && dense[pos] == entity;
	}

	T* get(Entity entity) {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 pos = sparse[idx];

		if (pos == INVALID_ENTITY || dense[pos] != entity)
			return nullptr;

		return &components[pos];
	}

	const T* get(Entity entity) const {
		Uint32 idx = Detail::entityIndex(entity);
		Uint32 pos = sparse[idx];

		if (pos == INVALID_ENTITY || dense[pos] != entity)
			return nullptr;

		return &components[pos];
	}

	size_t size() const override { return components.size(); }
	const std::vector<Entity>& entities() const override { return dense; }

	T& getByIndex(size_t i) { return components[i]; }
	const T& getByIndex(size_t i) const { return components[i]; }
};

} // namespace Blackthorn::ECS
