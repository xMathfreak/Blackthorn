#pragma once

#include <functional>
#include <unordered_map>

#include "Core/Export.h"
#include "ECS/Detail.h"
#include "Net/ByteBuffer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Non-intrusive serialization contract for ECS components.
 *
 * This template defines the interface for serializing and deserializing
 * ECS component types without modifying the component definitions themselves.
 *
 * To add serialization support for a new component type, provide an explicit
 * full specialization of this template in a header under
 * `ECS/Components/Serialization/`. Include that header wherever the component
 * needs to be serialized or deserialized.
 *
 * @tparam T The component type to be serialized.
 *
 * @details
 * Each specialization must implement exactly two static functions:
 *
 * @code
 * static void serialize(const T& component, Net::ByteBuffer& buf);
 * static void deserialize(T& component, Net::ByteBuffer& buf);
 * @endcode
 *
 * Requirements:
 * - `serialize` must write a fixed, deterministic number of bytes.
 * - `deserialize` must read exactly what `serialize` wrote, in the same order.
 * - There are no length prefixes between components inside a snapshot.
 * - The layout is fully determined by the registered component mask.
 *
 * @par Example
 * Example specialization (in ECS/Components/Serialization/MyComponent.h):
 *
 * @code
 * #include "ECS/Components/MyComponent.h"
 * #include "ECS/Serialization/ComponentSerializer.h"
 *
 * template <>
 * struct Blackthorn::ECS::Serialization::ComponentSerializer<MyComponent> {
 *     static void serialize(const MyComponent& c, Net::ByteBuffer& buf) {
 *         buf.writeF32(c.someField);
 *     }
 *     static void deserialize(MyComponent& c, Net::ByteBuffer& buf) {
 *         c.someField = buf.readF32();
 *     }
 * };
 * @endcode
 */
template <typename T>
struct ComponentSerializer {
	static void serialize(const T&, Net::ByteBuffer&) {
		static_assert(
			sizeof(T) == 0,
			"No ComponentSerializer specialization exists for this component type. "
			"Add one in ECS/Components/Serialization/<ComponentName>.h."
		);
	}

	static void deserialize(T&, Net::ByteBuffer&) {}
};

/**
 * @brief Registry for type-erased ECS component serializers.
 *
 * Maps a component's `Detail::componentID` to a pair of type-erased
 * serialize and deserialize functions. This allows systems such as
 * ComponentSnapshot to read and write components without knowing their
 * concrete types at the call site.
 *
 * @details
 * Each component type must be registered exactly once during engine startup,
 * after including its corresponding serializer specialization header.
 *
 * @code
 * #include "ECS/Components/Serialization/Transform.h"
 * // ...
 * SerializerRegistry::instance().registerComponent<Components::Transform>();
 * @endcode
 *
 * The registry internally stores a pair of lambdas that delegate to:
 * - ComponentSerializer<T>::serialize
 * - ComponentSerializer<T>::deserialize
 *
 * These functions are invoked through type-erased wrappers at runtime.
 */
class BLACKTHORN_API SerializerRegistry {
public:
	using SerializeFn   = std::function<void(const void*, Net::ByteBuffer&)>;
	using DeserializeFn = std::function<void(void*, Net::ByteBuffer&)>;

	struct Entry {
		SerializeFn   serialize;
		DeserializeFn deserialize;
	};

	static SerializerRegistry& instance() {
		static SerializerRegistry reg;
		return reg;
	}

	SerializerRegistry(const SerializerRegistry&) = delete;
	SerializerRegistry& operator=(const SerializerRegistry&) = delete;

	/**
	 * @brief Registers serialize/deserialize functions for component type T.
	 *
	 * Idempotent — registering the same type twice has no effect.
	 * A `ComponentSerializer<T>` specialization must be visible at the
	 * call site (i.e. its header must be included before calling this).
	 *
	 * @tparam T Component type with a ComponentSerializer specialization.
	 */
	template <typename T>
	void registerComponent() {
		const size_t id = Detail::componentID<T>();

		if (entries.count(id))
			return;

		entries[id] = Entry{
			[](const void* comp, Net::ByteBuffer& buf) {
				ComponentSerializer<T>::serialize(
					*static_cast<const T*>(comp), buf);
			},
			[](void* comp, Net::ByteBuffer& buf) {
				ComponentSerializer<T>::deserialize(
					*static_cast<T*>(comp), buf);
			}
		};
	}

	/**
	 * @brief Returns the serializer entry for a component by its ECS ID.
	 * @param componentId The value returned by Detail::componentID<T>().
	 * @return Pointer to the entry, or nullptr if not registered.
	 */
	const Entry* getEntry(size_t componentId) const {
		auto it = entries.find(componentId);
		return it != entries.end() ? &it->second : nullptr;
	}

	/**
	 * @brief Returns true if the given component ID has been registered.
	 */
	bool isRegistered(size_t componentId) const {
		return entries.count(componentId) > 0;
	}

private:
	SerializerRegistry() = default;

	std::unordered_map<size_t, Entry> entries;
};

} // namespace Blackthorn::ECS::Serialization