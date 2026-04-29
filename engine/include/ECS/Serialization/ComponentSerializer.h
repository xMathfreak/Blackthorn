#pragma once

#include <concepts>
#include <functional>
#include <unordered_map>

#include "Core/Export.h"
#include "ECS/Detail.h"
#include "IO/ByteBuffer.h"

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
 * static void serialize(const T& component, IO::ByteBuffer& buf);
 * static void deserialize(T& component, IO::ByteBuffer& buf);
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
 *     static void serialize(const MyComponent& c, IO::ByteBuffer& buf) {
 *         buf.writeF32(c.someField);
 *     }
 *     static void deserialize(MyComponent& c, IO::ByteBuffer& buf) {
 *         c.someField = buf.readF32();
 *     }
 * };
 * @endcode
 */
template <typename T>
struct ComponentSerializer {
	static void serialize(const T&, IO::ByteBuffer&) {
		static_assert(
			sizeof(T) == 0,
			"No ComponentSerializer specialization exists for this component type. "
			"Add one in ECS/Components/Serialization/<ComponentName>.h."
		);
	}

	static void deserialize(T&, IO::ByteBuffer&) {}
};

/**
 * @brief Bitmask controlling which pipelines a component participates in.
 *
 * Components default to @c Network only to preserve existing behaviour.
 * Opt a component into save serialization by passing @c Save or @c Both
 * to @c SerializerRegistry::registerComponent().
 */
enum class SerializationContext : U8 {
	Network = 1 << 0, ///< Component is serialized for network snapshots.
	Save = 1 << 1, ///< Component is serialized for save documents.
	Both = Network | Save,
};

inline SerializationContext operator|(SerializationContext a, SerializationContext b) {
	return static_cast<SerializationContext>(
		static_cast<U8>(a) | static_cast<U8>(b)
	);
}

inline bool hasContext(SerializationContext flags, SerializationContext flag) {
	return (static_cast<U8>(flags) & static_cast<U8>(flag)) != 0;
}

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
	using SerializeFn   = std::function<void(const void*, IO::ByteBuffer&)>;
	using DeserializeFn = std::function<void(void*, IO::ByteBuffer&)>;

	struct Entry {
		SerializeFn serialize;
		DeserializeFn deserialize;

		/**
		 * @brief Fixed wire size in bytes, or 0 for variable-length components.
		 *
		 * When non-zero, `ComponentSnapshotReader::skipComponents()` uses
		 * `ByteBuffer::skip()` to advance past this component in O(1) without
		 * deserializing. When zero, a full deserialize into a discard target
		 * is used instead to correctly advance past variable-length fields
		 * such as strings.
		 *
		 * Set automatically by `registerComponent<T>()` via
		 * `ComponentSerializer<T>::fixedSize()` if that static function exists,
		 * otherwise defaults to 0.
		 */
		size_t fixedSize = 0;

		SerializationContext context = SerializationContext::Network;
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
	 * Idempotent - registering the same type twice has no effect.
	 * A `ComponentSerializer<T>` specialization must be visible at the
	 * call site (i.e. its header must be included before calling this).
	 *
	 * @tparam T Component type with a ComponentSerializer specialization.
	 */
	template <typename T>
	void registerComponent(SerializationContext context = SerializationContext::Network) {
		const size_t id = Detail::componentID<T>();

		if (entries.count(id))
			return;

		Entry entry;

		entry.serialize = [](const void* comp, IO::ByteBuffer& buf) {
			ComponentSerializer<T>::serialize(*static_cast<const T*>(comp), buf);
		};

		entry.deserialize = [](void* comp, IO::ByteBuffer& buf) {
			ComponentSerializer<T>::deserialize(*static_cast<T*>(comp), buf);
		};

		if constexpr (requires { { ComponentSerializer<T>::fixedSize() } -> std::convertible_to<size_t>; }) {
			entry.fixedSize = ComponentSerializer<T>::fixedSize();
		} else {
			entry.fixedSize = 0;
		}

		entry.context = context;
		entries[id] = std::move(entry);
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

	/**
	 * @brief Returns true if the component is registered for the given context.
	 */
	bool isRegisteredFor(size_t componentId, SerializationContext ctx) const {
		auto it = entries.find(componentId);
		if (it == entries.end())
			return false;

		return hasContext(it->second.context, ctx);
	}

private:
	SerializerRegistry() = default;

	std::unordered_map<size_t, Entry> entries;
};

} // namespace Blackthorn::ECS::Serialization