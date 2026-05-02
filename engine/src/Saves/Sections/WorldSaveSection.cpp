#include "Saves/Sections/WorldSaveSection.h"

#include "Debug/Logger.h"
#include "ECS/Components/Persistent.h"
#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::Saves::Sections {

void WorldSaveSection::registerTypes() {
	ECS::Serialization::SerializerRegistry::instance()
		.pinType<ECS::Components::Persistent>();
}

void WorldSaveSection::write(SectionWriteContext& ctx) {
	const auto& reg = ECS::Serialization::SerializerRegistry::instance();
	const auto& entityData = pool.getEntities();

	ctx.buffer.writeU64(registry.nextAssignedId());

	const size_t countOffset = ctx.buffer.size();
	ctx.buffer.writeU32(0);

	U32 entityCount = 0;

	for (U32 idx = 0; idx < static_cast<U32>(entityData.size()); ++idx) {
		const auto& ed = entityData[idx];

		if (ed.componentMask == 0)
			continue;

		const ECS::Entity entity = ECS::Detail::makeEntity(idx, ed.generation);

		const auto* persistent = pool.getComponent<ECS::Components::Persistent>(entity);
		if (!persistent)
			continue;

		writeEntity(ctx.buffer, entity, *persistent, reg);
		++entityCount;
	}

	ctx.buffer.patchU32(countOffset, entityCount);
}

void WorldSaveSection::writeEntity(
	IO::ByteBuffer& buf,
	ECS::Entity entity,
	const ECS::Components::Persistent& persistent,
	const ECS::Serialization::SerializerRegistry& reg
) const {
	buf.writeU64(persistent.saveId);
	buf.writeU64(persistent.nameHash);

	const U64 fullMask = pool.getEntities()[ECS::Detail::entityIndex(entity)].componentMask;
	U64 saveMask = 0;

	for (size_t i = 0; i < ECS::Detail::MAX_COMPONENTS; ++i) {
		if ((fullMask & (1ULL << i)) != 0
			&& reg.isRegisteredFor(i, ECS::Serialization::SerializationContext::Save)
		) {
			saveMask |= (1ULL << i);
		}
	}

	buf.writeU64(saveMask);

	for (size_t i = 0; i < ECS::Detail::MAX_COMPONENTS; ++i) {
		if (!(saveMask & (1ULL << i)))
			continue;

		const auto* entry = reg.getEntry(i);
		const void* comp = pool.getComponentRaw(entity, i);

		if (comp)
			entry->serialize(comp, buf);
	}
}

void WorldSaveSection::read(SectionReadContext& ctx) {
	const auto& reg = ECS::Serialization::SerializerRegistry::instance();

	const U64 nextId = ctx.buffer.readU64();
	const U32 entityCount = ctx.buffer.readU32();

	registry.restoreNextId(nextId);

	for (U32 i = 0; i < entityCount; ++i)
		readEntity(ctx.buffer, reg);
}

void WorldSaveSection::readEntity(
	IO::ByteBuffer& buf,
	const ECS::Serialization::SerializerRegistry& reg
) {
	const U64 saveId = buf.readU64();
	const U64 nameHash = buf.readU64();
	const U64 saveMask = buf.readU64();

	ECS::Entity entity = pool.create();

	auto& persistent = pool.addComponent<ECS::Components::Persistent>(entity);
	persistent.saveId = saveId;
	persistent.nameHash = nameHash;

	registry.map(saveId, entity);

	for (size_t i = 0; i < ECS::Detail::MAX_COMPONENTS; ++i) {
		if (!(saveMask & (1ULL << i)))
			continue;

		const auto* entry = reg.getEntry(i);
		if (!entry) {
			BT_WARN(
				"WorldSaveSection: component bit {} has no registered entry "
				", save written by a newer build? Cannot advance stream safely, "
				"aborting component load for this entity",
				i
			);

			registry.remove(entity);
			pool.destroy(entity);
			return;
		}

		void* comp = pool.getComponentRaw(entity, i);

		if (!comp) {
			if (entry->construct)
				comp = entry->construct(pool, entity);

			if (!comp) {
				if (entry->fixedSize > 0) {
					buf.skip(entry->fixedSize);
					continue;
				}

				BT_WARN(
					"WorldSaveSection: component bit {} construct returned null "
					"and has no fixed size destroying partially-loaded entity.",
					i
				);

				registry.remove(entity);
				pool.destroy(entity);
				return;
			}
		}

		entry->deserialize(comp, buf);
	}
}

} // namespace Blackthorn::Saves::Sections