#pragma once

#include <concepts>
#include <functional>
#include <unordered_map>

#include "Core/Export.h"
#include "ECS/Detail.h"
#include "ECS/EntityPool.h"
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
	static void serialize(const T&, IO::ByteBuffer&);
	static void deserialize(T&, IO::ByteBuffer&);
};

/**
 * @brief Satisfied only when @c ComponentSerializer<T> has been fully
 * specialized with concrete @c serialize and @c deserialize implementations.
 *
 * Used by @c SerializerRegistry::registerComponent<T>() to select between
 * storing real serializer lambdas (specialized types) and a pure ID-pin
 * entry (unspecialized types such as @c Persistent that participate in the
 * ECS pool but carry no serialized payload of their own).
 */
template <typename T>
concept HasSerializerSpecialization = requires(const T& ct, T& t, IO::ByteBuffer& buf) {
	{ ComponentSerializer<T>::serialize(ct, buf) };
	{ ComponentSerializer<T>::deserialize(t, buf) };
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
	using SerializeFn = std::function<void(const void*, IO::ByteBuffer&)>;
	using DeserializeFn = std::function<void(void*, IO::ByteBuffer&)>;
	using ConstructFn = std::function<void*(EntityPool&, Entity)>;

	struct Entry {
		SerializeFn serialize;
		DeserializeFn deserialize;
		ConstructFn construct;

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

	/**
	 * @brief Single-definition singleton accessor for @c SerializerRegistry.
	 */
	static BLACKTHORN_API SerializerRegistry& instance();

	SerializerRegistry(const SerializerRegistry&) = delete;
	SerializerRegistry& operator=(const SerializerRegistry&) = delete;

	/**
	 * @brief Registers full serialize/deserialize functions for component type @c T.
	 *
	 * Requires a @c ComponentSerializer<T> specialization to be visible at the
	 * call site. Idempotent — registering the same type twice has no effect.
	 *
	 * Use this for components that carry serialized payload (network snapshots,
	 * save documents, or both). For components that must participate in the ECS
	 * pool but carry no payload — such as @c Persistent — use @c pinType<T>()
	 * instead to avoid a compile error from the unspecialized base template.
	 *
	 * @tparam T Component type with a @c ComponentSerializer specialization.
	 */
	template <HasSerializerSpecialization T>
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

		entry.construct = [](EntityPool& pool, Entity entity) -> void* {
			return &pool.addComponent<T>(entity);
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
	 * @brief Pins the component ID for type @c T without storing serializer
	 * functions.
	 *
	 * Calling this ensures @c Detail::componentID<T>() is evaluated from a
	 * controlled startup path — via the single exported @c nextComponentID()
	 * counter — before any @c EntityPool operation touches the type. This
	 * prevents the DLL-boundary hazard where @c addComponent and
	 * @c getComponent independently trigger ID assignment and receive
	 * different values.
	 *
	 * Use this for components like @c Persistent that must be pool-addressable
	 * but whose payload is handled explicitly by the calling code rather than
	 * through the @c SerializerRegistry component mask.
	 *
	 * Idempotent — safe to call multiple times.
	 *
	 * @tparam T Any component type. No @c ComponentSerializer specialization
	 *           required.
	 */
	template <typename T>
	void pinType() {
		const size_t id = Detail::componentID<T>();

		if (entries.count(id))
			return;

		Entry entry;
		entry.serialize = nullptr;
		entry.deserialize = nullptr;
		entry.fixedSize = 0;
		entry.context = SerializationContext::Network;
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