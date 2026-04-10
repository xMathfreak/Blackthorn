#pragma once

#include "ECS/Components/Transform.h"
#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Serializer specialization for `Components::Transform`.
 *
 * Wire layout (16 bytes, little-endian):
 * @code
 * float position.x
 * float position.y
 * float angle
 * float scale
 * @endcode
 */
template <>
struct ComponentSerializer<Components::Transform> {
	static void serialize(const Components::Transform& c, Net::ByteBuffer& buf) {
		buf.writeF32(c.position.x);
		buf.writeF32(c.position.y);
		buf.writeF32(c.angle);
		buf.writeF32(c.scale);
	}

	static void deserialize(Components::Transform& c, Net::ByteBuffer& buf) {
		c.position.x = buf.readF32();
		c.position.y = buf.readF32();
		c.angle = buf.readF32();
		c.scale = buf.readF32();
	}
};

} // namespace Blackthorn::ECS::Serialization