#pragma once

#include <cassert>
#include <typeindex>

#include "Core/Export.h"
#include "ECS/Entity.h"

namespace Blackthorn::ECS::Detail {

constexpr U32 MAX_ENTITIES = 8192;
constexpr U8 INDEX_BITS = 24;
constexpr U32 INDEX_MASK = (1u << INDEX_BITS) - 1;
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

/**
 * @brief Returns or assigns a stable component type index for @p type.
 *
 * @internal Use @c componentID<T>() at call sites, not this function directly.
 */
BLACKTHORN_API size_t componentIDForType(std::type_index type) noexcept;

/**
 * @brief Returns the stable per-process index for component type @c T.
 */
template <typename T>
inline size_t componentID() noexcept {
	const size_t id = componentIDForType(typeid(T));

	#ifdef BLACKTHORN_DEBUG
		assert(id < MAX_COMPONENTS);
	#endif

	return id;
}

template <typename T>
inline U64 componentMask() noexcept {
	const size_t id = componentID<T>();

	#ifdef BLACKTHORN_DEBUG
		assert(id < MAX_COMPONENTS);
	#endif

	return (1ULL << id);
}

template <typename T>
using RawType = std::remove_cv_t<std::remove_pointer_t<T>>;

} // namespace Blackthorn::ECS::Detail