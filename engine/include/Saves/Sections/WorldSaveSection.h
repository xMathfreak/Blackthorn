#pragma once

#include <string_view>

#include "Core/Export.h"
#include "ECS/EntityPool.h"
#include "ECS/Serialization/ComponentSerializer.h"
#include "Saves/SaveEntityRegistry.h"
#include "Saves/SaveHash.h"
#include "Saves/Sections/ISaveSection.h"

namespace Blackthorn::Saves::Sections {

/**
 * @brief Built-in save section for ECS world entity state (@c bt.world).
 *
 * Serializes all entities carrying the @c Components::Persistent component,
 * writing their @c SaveEntityId, stable name hash, component mask, and
 * component data for every component registered with @c SerializationContext::Save
 * in the @c SerializerRegistry.
 *
 * @par Wire layout (payload, after section table lookup)
 * @code
 * [uint64 nextSaveId]         # SaveEntityRegistry next ID counter
 * [uint32 entityCount]
 * per entity:
 *   [uint64 saveEntityId]
 *   [uint64 nameHash]
 *   [uint64 componentMask]    # only bits set for save-registered components
 *   per set bit i (low to high):
 *     [N bytes component data] # via ComponentSerializer<T>::serialize
 * @endcode
 *
 * @par Loading
 * On load, @c read() creates entities in the pool, assigns their
 * @c Persistent component, maps them in the registry, and deserializes
 * component data. The caller is responsible for ensuring the pool is clear
 * before loading.
 *
 * @par SerializerRegistry context
 * Only components registered with @c SerializationContext::Save (or @c Both)
 * are included. Components registered for @c Network only are skipped.
 * This separation is intentional — not all networked state needs to persist.
 */
class BLACKTHORN_API WorldSaveSection final : public ISaveSection {
public:
	static constexpr std::string_view SECTION_NAME = "bt.world";
	static constexpr U32 CURRENT_VERSION = 1;

	/**
	 * @brief Registers built-in component types required by this section.
	 *
	 * Must be called once during engine startup — before any @c EntityPool
	 * operation touches these types — so that @c `Detail::componentID<T>()`
	 * assigns IDs from the single exported counter while all modules are
	 * still in the same call stack. This prevents the DLL boundary hazard
	 * where @c addComponent and @c getComponent resolve to different IDs
	 * for the same type.
	 *
	 * Registers:
	 * - @c ECS::Components::Persistent — required for the save filter in
	 *   @c write() and for @c addComponent in @c readEntity(). It carries
	 *   no serialized payload of its own; registration here is purely to
	 *   pin its component ID before the pool is used.
	 *
	 * Calling this more than once is safe — subsequent calls are no-ops.
	 */
	static void registerTypes();

	/**
	 * @brief Constructs a world section operating on @p pool and @p registry.
	 *
	 * Both references must outlive this section instance.
	 *
	 * @param ep Entity pool to read from (write) or populate (read).
	 * @param reg Save entity registry to use for ID assignment and mapping.
	 */
	WorldSaveSection(ECS::EntityPool& ep, SaveEntityRegistry& reg)
		: pool(ep)
		, registry(reg)
	{}

	U64 getId() const override { return saveHash(SECTION_NAME); }
	std::string_view getName() const override { return SECTION_NAME; }
	U32 getVersion() const override { return CURRENT_VERSION; }

	void write(SectionWriteContext& ctx) override;
	void read(SectionReadContext& ctx) override;

private:
	ECS::EntityPool& pool;
	SaveEntityRegistry& registry;

	void writeEntity(
		IO::ByteBuffer& buf,
		ECS::Entity entity,
		const ECS::Serialization::SerializerRegistry& reg
	) const;

	void readEntity(
		IO::ByteBuffer& buf,
		const ECS::Serialization::SerializerRegistry& reg
	);
};

} // namespace Blackthorn::Saves::Sections