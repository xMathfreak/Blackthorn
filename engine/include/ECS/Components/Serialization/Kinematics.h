#pragma once

#include "ECS/Components/Kinematics.h"
#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Serializer specialization for `Components::Kinematics`.
 *
 * Wire layout (16 bytes, little-endian):
 * @code
 * float oldPosition.x
 * float oldPosition.y
 * float acceleration.x
 * float acceleration.y
 * @endcode
 *
 * @note `oldPosition` is included because a server running Verlet
 * integration needs it to reproduce the next tick's result identically.
 */
template <>
struct ComponentSerializer<Components::Kinematics> {
	static constexpr size_t fixedSize() { return 4 * sizeof(float); }

	static void serialize(const Components::Kinematics& c, Net::ByteBuffer& buf) {
		buf.writeF32(c.oldPosition.x);
		buf.writeF32(c.oldPosition.y);
		buf.writeF32(c.acceleration.x);
		buf.writeF32(c.acceleration.y);
	}

	static void deserialize(Components::Kinematics& c, Net::ByteBuffer& buf) {
		c.oldPosition.x = buf.readF32();
		c.oldPosition.y = buf.readF32();
		c.acceleration.x = buf.readF32();
		c.acceleration.y = buf.readF32();
	}
};

} // namespace Blackthorn::ECS::Serialization