#pragma once

#include <atomic>
#include <cassert>

#include "ECS/Entity.h"

namespace Blackthorn::ECS::Detail {
	constexpr U32 MAX_ENTITIES = 8192;
	constexpr U8 INDEX_BITS = 24;
	constexpr U32 INDEX_MASK = (1u << INDEX_BITS) - 1;
	constexpr U32 GENERATION_BITS = 32 - INDEX_BITS;
	constexpr size_t MAX_COMPONENTS = 64;

	inline U32 entityIndex(Entity e) noexcept {
		return e & INDEX_MASK;
	}

	inline U8 entityGeneration(Entity e) noexcept {
		return static_cast<U8>(e >> INDEX_BITS);
	}

	inline Entity makeEntity(U32 index, U8 generation) noexcept {
		return (static_cast<Entity>(generation) << INDEX_BITS) | (index & INDEX_MASK);
	}

	inline size_t nextComponentID() {
		static std::atomic<size_t> id{0};
		return id.fetch_add(1, std::memory_order::relaxed);
	}

	template <typename T>
	inline size_t componentID() {
		static const size_t id = nextComponentID();

		#ifdef BLACKTHORN_DEBUG
			assert(id < MAX_COMPONENTS);
		#endif

		return id;
	}

	template <typename T>
	inline U64 componentMask() noexcept {
		size_t id = componentID<T>();
		assert(id < MAX_COMPONENTS);
		return (1ULL << id);
	}

	template <typename T>
	using RawType = std::remove_cv_t<std::remove_pointer_t<T>>;

} // namespace Blackthorn::ECS::Detail