#pragma once

#include "ECS/Components/Sprite.h"
#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::ECS::Serialization {

/**
 * @brief Serializer specialization for `Components::Sprite`.
 *
 * Wire layout (12 bytes, little-endian):
 * @code
 * float width
 * float height
 * float zOrder
 * @endcode
 *
 * @note The `texture` pointer is intentionally excluded. The receiving
 * end resolves the texture from an asset ID carried in the spawn message
 * for this entity, then assigns it locally after the asset is loaded.
 */
template <>
struct ComponentSerializer<Components::Sprite> {
	static constexpr size_t fixedSize() { return 3 * sizeof(float); }

	static void serialize(const Components::Sprite& c, Net::Core::ByteBuffer& buf) {
		buf.writeF32(c.width);
		buf.writeF32(c.height);
		buf.writeF32(c.zOrder);
	}

	static void deserialize(Components::Sprite& c, Net::Core::ByteBuffer& buf) {
		c.width = buf.readF32();
		c.height = buf.readF32();
		c.zOrder = buf.readF32();
	}
};

} // namespace Blackthorn::ECS::Serialization