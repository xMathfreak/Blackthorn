#pragma once

#include <ECS/Entity.h>

#include <atomic>
#include <cassert>

namespace Blackthorn::ECS::Detail {
	constexpr Uint32 MAX_ENTITIES = 8192;
	constexpr Uint8 INDEX_BITS = 24;
	constexpr Uint32 INDEX_MASK = (1u << INDEX_BITS) - 1;
	constexpr Uint32 GENERATION_BITS = 32 - INDEX_BITS;
	constexpr size_t MAX_COMPONENTS = 64;

	inline Uint32 entityIndex(Entity e) noexcept {
		return e & INDEX_MASK;
	}

	inline Uint8 entityGeneration(Entity e) noexcept {
		return static_cast<Uint8>(e >> INDEX_BITS);
	}

	inline Entity makeEntity(Uint32 index, Uint8 generation) noexcept {
		return (static_cast<Entity>(generation) << INDEX_BITS) | (index & INDEX_MASK);
	}

	inline size_t nextComponentID() {
		static std::atomic<size_t> id{0};
		return id.fetch_add(1, std::memory_order_relaxed);
	}

	template <typename T>
	inline size_t componentID() {
		static size_t id = nextComponentID();
		return id;
	}

	template <typename T>
	inline Uint64 componentMask() noexcept {
		size_t id = componentID<T>();
		assert(id < MAX_COMPONENTS);
		return (1ULL << id);
	}

	template <typename T>
	using RawType = std::remove_cv_t<std::remove_pointer_t<T>>;

} // namespace Blackthorn::ECS::Detail