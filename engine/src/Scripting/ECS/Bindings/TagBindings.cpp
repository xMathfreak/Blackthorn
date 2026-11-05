#include "Blackthorn/Scripting/ECS/BuiltinBindings.hpp"

#include "Blackthorn/ECS/Components/Tag.hpp"
#include "Blackthorn/Scripting/ECS/ComponentBindings.hpp"
#include "Blackthorn/Scripting/ECS/ScriptWorld.hpp"

namespace Blackthorn::Scripting::ECS::Bindings {

void registerTagBindings(Scripting::ECS::ComponentBindings& bindings) {
	bindings.declareComponent<Blackthorn::ECS::Components::Tag>("tag");

	bindings.registerGetter<Blackthorn::ECS::Components::Tag>("values",
		[](ScriptWorld& w, EntityHandle h) -> std::vector<std::string> {
			const Blackthorn::ECS::Components::Tag* tags = w.tryComponent<Blackthorn::ECS::Components::Tag>(h);
			return tags ? tags->values : std::vector<std::string>{};
		}
	);
}

} // namespace Blackthorn::Scripting::ECS::Bindings