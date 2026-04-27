#include "ECS/NetworkEntityRegistry.h"

#include "Net/Protocol/PacketWriter.h"

namespace Blackthorn::ECS {

void NetworkEntityRegistry::serializeSpawn(
	Net::Core::ByteBuffer& buf,
	NetworkEntityId netId,
	Uint32 tick
) const {
	if (netId == INVALID_NET_ENTITY)
		throw std::invalid_argument(
			"NetworkEntityRegistry::serializeSpawn: cannot serialise INVALID_NET_ENTITY"
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
	Net::Core::ByteBuffer& buf,
	NetworkEntityId netId,
	DespawnReason reason,
	Uint32 tick
) const {
	if (netId == INVALID_NET_ENTITY)
		throw std::invalid_argument(
			"NetworkEntityRegistry::serializeDespawn: cannot serialise INVALID_NET_ENTITY"
		);

	Net::Protocol::PacketWriter pw(
		buf,
		Net::Protocol::PacketType::EntityDestroy,
		tick
	);

	pw.buffer().writeU64(netId);
	pw.buffer().writeU8(static_cast<Uint8>(reason));

	pw.finish();
}

} // namespace Blackthorn::ECS