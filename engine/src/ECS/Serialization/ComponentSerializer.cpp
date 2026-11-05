#include "ECS/Serialization/ComponentSerializer.hpp"

namespace Blackthorn::ECS::Serialization {

SerializerRegistry& SerializerRegistry::instance() {
	static SerializerRegistry reg;
	return reg;
}

} // namespace Blackthorn::ECS::Serialization