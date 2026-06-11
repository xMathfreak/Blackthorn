#include "ECS/NetworkEntityRegistry.h"

#include "Net/Protocol/PacketWriter.h"

namespace Blackthorn::ECS {

void NetworkEntityRegistry::serializeSpawn(
	IO::ByteBuffer& buf,
	NetworkEntityId netId,
	U32 tick
) const {
	if (netId == INVALID_NET_ENTITY)
		throw std::invalid_argument(
			"NetworkEntityRegistry::serializeSpawn: cannot serialize INVALID_NET_ENTITY"
		);

	Net::Protocol::PacketWriter pw(
		buf,
		Net::Protocol::PacketType::EntityCreate,
		tick
	);

	pw.buffer().writeU64(netId);

	pw.finish();
}

void NetworkEntityRegistry::serializeDespawn(
	IO::ByteBuffer& buf,
	NetworkEntityId netId,
	DespawnReason reason,
	U32 tick
) const {
	if (netId == INVALID_NET_ENTITY)
		throw std::invalid_argument(
			"NetworkEntityRegistry::serializeDespawn: cannot serialize INVALID_NET_ENTITY"
		);

	Net::Protocol::PacketWriter pw(
		buf,
		Net::Protocol::PacketType::EntityDestroy,
		tick
	);

	pw.buffer().writeU64(netId);
	pw.buffer().writeU8(static_cast<U8>(reason));

	pw.finish();
}

} // namespace Blackthorn::ECS