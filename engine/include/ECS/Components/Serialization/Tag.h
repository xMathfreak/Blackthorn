#pragma once

#include "ECS/Components/Tag.h"
#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Serializer specialization for `Components::Tag`.
 *
 * Wire layout (variable length):
 * @code
 * uint16  nameLength   (number of UTF-8 bytes in the name string)
 * uint8[] nameBytes    (UTF-8 encoded string, no null terminator)
 * @endcode
 *
 * @note Tag is the only built-in component with a variable-length wire
 * representation. This means the snapshot system cannot skip it using a
 * fixed byte count and must deserialize it to advance the read cursor
 * correctly. See the note in ComponentSnapshot.h regarding the size-hint
 * optimization for future improvement.
 */
template <>
struct ComponentSerializer<Components::Tag> {
	static void serialize(const Components::Tag& c, IO::ByteBuffer& buf) {
		buf.writeString(c.name);
	}

	static void deserialize(Components::Tag& c, IO::ByteBuffer& buf) {
		c.name = buf.readString();
	}
};

} // namespace Blackthorn::ECS::Serialization