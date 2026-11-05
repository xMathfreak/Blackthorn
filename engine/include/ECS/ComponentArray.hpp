#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include "ECS/Detail.h"
#include "ECS/IComponentArray.h"

namespace Blackthorn::ECS {

/**
 * @brief Dense storage for one component type, indexed by entity.
 *
 * Components live contiguously in `components`. `dense` maps each component
 * back to its entity, and `sparse` maps an entity slot to its position. Each
 * sparse entry also stores the generation it was written for, so a stale
 * handle can't reach a component that now belongs to a reused slot.
 *
 * Pointers and references returned by get() and insert() are invalidated by
 * any insert or remove on this array.
 */
template <typename T>
class ComponentArray : public IComponentArray {
private:
	static constexpr U32 kNoPos = U32_MAX;

	struct SparseEntry {
		U32 pos = kNoPos;
		U16 generation = 0;
	};

	std::vector<T> components;
	std::vector<Entity> dense;
	std::vector<SparseEntry> sparse;

public:
	/**
	 * @param capacity Number of entity slots to size the sparse table for.
	 *                 Pass the owning pool's capacity so inserts never grow it.
	 */
	explicit ComponentArray(size_t capacity = Detail::MAX_ENTITIES) {
		components.reserve(capacity);
		dense.reserve(capacity);
		sparse.resize(capacity);
	}

	/**
	 * @brief Adds a component for @p entity, or replaces the existing one.
	 *
	 * If an entry from an older generation of the same slot is still present,
	 * it is removed first. Otherwise it would stay in `components` with no
	 * sparse entry pointing to it.
	 *
	 * Strong exception guarantee: if T's constructor throws, the array is unchanged.
	 *
	 * @return A reference valid until the next insert or remove on this array.
	 */
	template <typename... Args>
	T& insert(Entity entity, Args&&... args) {
		const U32 idx = Detail::entityIndex(entity);
		const U16 gen = Detail::entityGeneration(entity);

		if (idx >= sparse.size())
			sparse.resize(idx + 1);

		SparseEntry& entry = sparse[idx];

		// Drop stale generation first
		if (entry.pos != kNoPos && entry.generation != gen)
			removeAt(entry.pos, entry);

		if (entry.pos != kNoPos) {
			T replacement(std::forward<Args>(args)...);
			components[entry.pos] = std::move(replacement);
			return components[entry.pos];
		}

		const U32 pos = static_cast<U32>(components.size());

		T& component = components.emplace_back(std::forward<Args>(args)...);
		try {
			dense.push_back(entity);
		} catch (...) {
			components.pop_back();
			throw;
		}

		entry.pos = pos;
		entry.generation = gen;
		return component;
	}

	void remove(Entity entity) override {
		const U32 idx = Detail::entityIndex(entity);
		const U16 gen = Detail::entityGeneration(entity);

		if (idx >= sparse.size())
			return;

		SparseEntry& entry = sparse[idx];
		if (entry.pos == kNoPos || entry.generation != gen)
			return;

		removeAt(entry.pos, entry);
	}

	bool has(Entity entity) const override {
		return find(entity) != nullptr;
	}

	T* get(Entity entity) {
		const SparseEntry* entry = find(entity);
		return entry ? &components[entry->pos] : nullptr;
	}

	const T* get(Entity entity) const {
		const SparseEntry* entry = find(entity);
		return entry ? &components[entry->pos] : nullptr;
	}

	void* getRaw(Entity entity) override {
		return static_cast<void*>(get(entity));
	}

	const void* getRaw(Entity entity) const override {
		return static_cast<const void*>(get(entity));
	}

	size_t size() const override { return components.size(); }
	const std::vector<Entity>& entities() const override { return dense; }

	T& getByIndex(size_t i) { return components[i]; }
	const T& getByIndex(size_t i) const { return components[i]; }

	T* data() noexcept { return components.data(); }
	const T* data() const noexcept { return components.data(); }

private:
	/// Returns the sparse entry for @p entity if it holds a live component, else null.
	const SparseEntry* find(Entity entity) const noexcept {
		const U32 idx = Detail::entityIndex(entity);
		if (idx >= sparse.size())
			return nullptr;

		const SparseEntry& entry = sparse[idx];
		const bool live = entry.pos != kNoPos
			&& entry.generation == Detail::entityGeneration(entity);

		return live ? &entry : nullptr;
	}

	/**
	 * @brief Removes the component at @p pos by moving the last element into
	 *        its place, then clears @p entry.
	 *
	 * The entity that was moved gets its sparse position updated. Its
	 * generation is already correct, because the dense array holds the full
	 * entity value.
	 */
	void removeAt(U32 pos, SparseEntry& entry) {
		assert(pos < components.size());

		const U32 last = static_cast<U32>(components.size() - 1);
		if (pos != last) {
			components[pos] = std::move(components[last]);
			const Entity moved = dense[last];
			dense[pos] = moved;
			sparse[Detail::entityIndex(moved)].pos = pos;
		}

		components.pop_back();
		dense.pop_back();
		entry.pos = kNoPos;
	}
};

} // namespace Blackthorn::ECS