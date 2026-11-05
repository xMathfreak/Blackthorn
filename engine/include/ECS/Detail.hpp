#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

#include "Core/Export.h"
#include "ECS/Entity.h"
#include "Types/Numeric.h"

namespace Blackthorn::ECS::Detail {

constexpr U32 MAX_ENTITIES = 32768;
constexpr size_t MAX_COMPONENTS = 64;

/// Bits for the slot index. The remaining bits of an Entity hold the generation.
constexpr U8 INDEX_BITS = 16;
constexpr U8 GENERATION_BITS = 16;
constexpr U32 INDEX_MASK = (1u << INDEX_BITS) - 1;

static_assert(INDEX_BITS > 0 && INDEX_BITS < 32);

static_assert(INDEX_BITS + GENERATION_BITS == 32,
	"Entity is 32 bits: index and generation must fill it exactly");

static_assert(MAX_ENTITIES <= (U32{1} << INDEX_BITS),
	"Entity capacity exceeds INDEX_BITS");

/// Slot index of an entity. Valid for any Entity, including INVALID_ENTITY.
inline U32 entityIndex(Entity e) noexcept {
	return e & INDEX_MASK;
}

/// Generation of an entity. Increments each time its slot is reused.
inline U16 entityGeneration(Entity e) noexcept {
	return static_cast<U16>(e >> INDEX_BITS);
}

inline Entity makeEntity(U32 index, U16 generation) noexcept {
	assert(index <= INDEX_MASK);
	return (static_cast<Entity>(generation) << INDEX_BITS) | (index & INDEX_MASK);
}

/**
 * @brief Assigns the next free component index for @p type, or returns the
 *        existing one.
 *
 * @throws std::length_error if more than MAX_COMPONENTS types are registered.
 * @internal Use componentID<T>() at call sites.
 */
BLACKTHORN_API size_t assignComponentID(std::type_index type);

/**
 * @brief Stable per-process index for component type @c T.
 *
 * Assigned on first call and cached, so repeated calls cost one load.
 *
 * @throws std::length_error on the first call that would exceed MAX_COMPONENTS.
 */
template <typename T>
inline size_t componentID() {
	static const size_t id = assignComponentID(typeid(T));
	return id;
}

/// Bit for component @c T in an entity's componentMask.
template <typename T>
inline U64 componentMask() {
	return U64{ 1 } << componentID<T>();
}

template <typename T>
using RawType = std::remove_cv_t<std::remove_pointer_t<T>>;

} // namespace Blackthorn::ECS::Detail