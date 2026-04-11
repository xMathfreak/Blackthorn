#pragma once

#include <functional>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "ECS/EntityPool.h"
#include "ECS/Serialization/ComponentSerializer.h"
#include "Net/ByteBuffer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Network entity ID type.
 *
 * A stable, explicitly assigned identifier that is meaningful across
 * machines.
 * Distinct from the local ECS `Entity` handle, which is a
 * machine-local index + generation value.
 */
using NetworkEntityId = Uint32;

static constexpr NetworkEntityId INVALID_NET_ENTITY = UINT32_MAX;

/**
 * @brief Describes a single entity entry read from a snapshot.
 *
 * Returned by `ComponentSnapshotReader::readNext()` so callers can
 * process one entity at a time without building a large intermediate
 * collection.
 */
struct SnapshotEntity {
	NetworkEntityId netId = INVALID_NET_ENTITY;
	Uint64 componentMask = 0;

	/// Byte offset into the source ByteBuffer at which this entity's
	/// component data begins. Valid only for the lifetime of that buffer.
	size_t componentDataOffset = 0;
};

/**
 * @brief Writes a full entity snapshot payload into a ByteBuffer.
 *
 * Snapshot wire layout (raw binary, no field tags):
 * @code
 * [uint64  tick]
 * [uint32  entityCount]
 * per entity:
 *   [uint32  networkId]
 *   [uint64  componentMask]
 *   per set bit i in componentMask (low to high):
 *     [N bytes — fixed layout per ComponentSerializer<T> specialization]
 * @endcode
 *
 * This payload is intended to follow a `Net::PacketHeader` with
 * `packetType == Net::PacketType::Snapshot`.
 *
 * Only components registered with `SerializerRegistry` are written.
 * Components present on an entity but not registered are silently skipped
 * and their bit is cleared from the written componentMask.
 *
 * @code
 * Net::ByteBuffer buf;
 * buf.reserve(4096);
 *
 * // Write packet header first (payloadLength back-patched below).
 * Net::PacketHeader header;
 * header.packetType = Net::PacketType::Snapshot;
 * header.tick       = simClock.getCurrentTick();
 * size_t headerStart = buf.size();
 * header.serialize(buf);
 *
 * // Write snapshot payload.
 * size_t payloadStart = buf.size();
 * ComponentSnapshotWriter writer(pool, netIdLookup, simClock.getCurrentTick());
 * writer.write(buf);
 *
 * // Back-patch the payload length field in the header.
 * Uint32 payloadLen = static_cast<Uint32>(buf.size() - payloadStart);
 * buf.patchU32(headerStart + 16, payloadLen);
 * @endcode
 */
class BLACKTHORN_API ComponentSnapshotWriter {
public:
	/**
	 * @brief Callback type used to look up a NetworkEntityId for a local Entity.
	 *
	 * The writer calls this for every candidate entity. Return
	 * `INVALID_NET_ENTITY` to skip the entity (e.g. it has not yet been
	 * assigned a network ID).
	 */
	using NetIdLookup = std::function<NetworkEntityId(Entity)>;

	/**
	 * @brief Constructs a ComponentSnapshotWriter.
	 * @param pool   The entity pool to snapshot.
	 * @param lookup Callback mapping local Entity to NetworkEntityId.
	 * @param tick   The simulation tick this snapshot represents.
	 */
	ComponentSnapshotWriter(
		EntityPool& pool,
		NetIdLookup lookup,
		Uint64 tick
	)
		: pool(pool)
		, lookup(std::move(lookup))
		, tick(tick)
	{}

	/**
	 * @brief Writes the full snapshot payload into `buf`.
	 * @param buf Destination buffer. The packet header must already have
	 *            been written before calling this.
	 */
	void write(Net::ByteBuffer& buf) const {
		const auto& registry = SerializerRegistry::instance();

		buf.writeU64(tick);

		const size_t countOffset = buf.size();
		buf.writeU32(0);

		Uint32 entityCount = 0;

		const auto& entityData = pool.getEntities();
		for (Uint32 idx = 0; idx < static_cast<Uint32>(entityData.size()); ++idx) {
			const auto& ed = entityData[idx];

			if (ed.componentMask == 0)
				continue;

			Entity localEntity = Detail::makeEntity(idx, ed.generation);

			if (!pool.isValid(localEntity))
				continue;

			NetworkEntityId netId = lookup(localEntity);
			if (netId == INVALID_NET_ENTITY)
				continue;

			Uint64 writeMask = buildWriteMask(ed.componentMask, registry);
			if (writeMask == 0)
				continue;

			buf.writeU32(netId);
			buf.writeU64(writeMask);
			writeComponents(buf, localEntity, writeMask, registry);

			++entityCount;
		}

		buf.patchU32(countOffset, entityCount);
	}

private:
	EntityPool& pool;
	NetIdLookup lookup;
	Uint64 tick;

	static Uint64 buildWriteMask(
		Uint64 componentMask,
		const SerializerRegistry& registry)
	{
		Uint64 writeMask = 0;
		for (size_t i = 0; i < Detail::MAX_COMPONENTS; ++i) {
			if ((componentMask & (1ULL << i)) && registry.isRegistered(i))
				writeMask |= (1ULL << i);
		}
		return writeMask;
	}

	void writeComponents(
		Net::ByteBuffer& buf,
		Entity entity,
		Uint64 writeMask,
		const SerializerRegistry& registry) const
	{
		for (size_t i = 0; i < Detail::MAX_COMPONENTS; ++i) {
			if (!(writeMask & (1ULL << i)))
				continue;

			const auto* entry = registry.getEntry(i);
			if (!entry)
				continue;

			const void* comp = pool.getComponentRaw(entity, i);
			if (comp)
				entry->serialize(comp, buf);
		}
	}
};

/**
 * @brief Reads a snapshot payload from a ByteBuffer.
 *
 * Iterates entity entries one at a time. The caller maps each
 * `NetworkEntityId` to a local entity and calls `applyComponents()` to
 * write the received state into the pool.
 *
 * @code
 * ComponentSnapshotReader reader(buf);
 * Uint64 tick = reader.tick();
 *
 * SnapshotEntity entry;
 * while (reader.readNext(entry)) {
 *     Entity local = netIdMap.toLocal(entry.netId);
 *     reader.applyComponents(entry, local, pool);
 * }
 * @endcode
 */
class BLACKTHORN_API ComponentSnapshotReader {
public:
	/**
	 * @brief Constructs a reader from a buffer positioned at the start of
	 * the snapshot payload (immediately after the PacketHeader).
	 */
	explicit ComponentSnapshotReader(Net::ByteBuffer& buf)
		: buf(buf)
		, snapshotTick(buf.readU64())
		, entityCount(buf.readU32())
	{}

	ComponentSnapshotReader(const ComponentSnapshotReader&) = delete;
	ComponentSnapshotReader& operator=(const ComponentSnapshotReader&) = delete;

	/** @brief Returns the simulation tick this snapshot was captured at. */
	Uint64 tick() const { return snapshotTick; }

	/** @brief Returns the total number of entity entries in this snapshot. */
	Uint32 count() const { return entityCount; }

	/**
	 * @brief Reads the next entity header into `out` and advances past its
	 * component data.
	 *
	 * @param out Receives the netId, componentMask, and componentDataOffset.
	 * @return true if an entity was read, false if the snapshot is exhausted.
	 */
	bool readNext(SnapshotEntity& out) {
		if (entitiesRead >= entityCount || buf.exhausted())
			return false;

		out.netId = buf.readU32();
		out.componentMask = buf.readU64();
		out.componentDataOffset = buf.readPosition();

		skipComponents(out.componentMask);

		++entitiesRead;
		return true;
	}

	/**
	 * @brief Applies the component data from `entry` to `entity` in `pool`.
	 *
	 * Creates a scoped ByteBuffer view starting at
	 * `entry.componentDataOffset` and deserializes each component in
	 * mask order into the matching component arrays in `pool`. Components
	 * not present in the pool are skipped.
	 *
	 * Safe to call before the next `readNext()` — it reads from the stored
	 * offset rather than the live cursor.
	 *
	 * @param entry  Snapshot entry returned by readNext().
	 * @param entity Local ECS entity to write component state into.
	 * @param pool   Entity pool that owns the component arrays.
	 */
	void applyComponents(
		const SnapshotEntity& entry,
		Entity entity,
		EntityPool& pool) const
	{
		const auto& registry = SerializerRegistry::instance();

		Net::ByteBuffer view(
			buf.data() + entry.componentDataOffset,
			buf.size() - entry.componentDataOffset
		);

		for (size_t i = 0; i < Detail::MAX_COMPONENTS; ++i) {
			if (!(entry.componentMask & (1ULL << i)))
				continue;

			const auto* regEntry = registry.getEntry(i);
			if (!regEntry)
				continue;

			void* comp = pool.getComponentRaw(entity, i);
			if (comp)
				regEntry->deserialize(comp, view);
		}
	}

private:
	Net::ByteBuffer& buf;
	Uint64 snapshotTick = 0;
	Uint32 entityCount = 0;
	Uint32 entitiesRead = 0;

	/**
	 * @brief Advances the buffer cursor past all component data for `mask`.
	 *
	 * For fixed-size components this is equivalent to a seek. For
	 * variable-size components (e.g. Tag/string) a full deserialize into
	 * a discard target is required to advance correctly.
	 *
	 * @note A future optimization is to store a per-component byte-size
	 * hint in SerializerRegistry::Entry for fixed-size components, allowing
	 * a direct seek without deserializing. Variable-size components would
	 * retain the current approach.
	 */
	void skipComponents(Uint64 mask) {
		const auto& registry = SerializerRegistry::instance();

		for (size_t i = 0; i < Detail::MAX_COMPONENTS; ++i) {
			if (!(mask & (1ULL << i)))
				continue;

			const auto* entry = registry.getEntry(i);
			if (!entry)
				continue;

			if (entry->fixedSize > 0) {
				buf.skip(entry->fixedSize);
			} else {
				const size_t before = buf.readPosition();
				Net::ByteBuffer discard(buf.data() + before, buf.remaining());
				entry->deserialize(discardSentinel(), discard);
				buf.skip(discard.readPosition());
			}
		}
	}

	/**
	 * @brief Returns a pointer to a static per-type discard target.
	 *
	 * Used exclusively by `skipComponents()` to give the deserialize
	 * function a valid (but immediately discarded) write target. Returns
	 * `nullptr` because the deserialize lambdas in SerializerRegistry
	 * cast the `void*` to their concrete type before writing — passing
	 * `nullptr` would crash them.
	 *
	 * Instead we allocate a small static scratch buffer aligned to
	 * `max_align_t` and large enough for any built-in component. If a
	 * user-defined variable-length component is larger, they must either
	 * provide a `fixedSize()` override or increase this size.
	 */
	static void* discardSentinel() {
		alignas(std::max_align_t) static thread_local std::byte scratch[256];
		return static_cast<void*>(scratch);
	}
};

} // namespace Blackthorn::ECS::Serialization