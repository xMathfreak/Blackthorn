#include "ECS/Serialization/ComponentSerializer.h"

namespace Blackthorn::ECS::Serialization {

SerializerRegistry& SerializerRegistry::instance() {
	static SerializerRegistry reg;
	return reg;
}

} // namespace Blackthorn::ECS::Serialization