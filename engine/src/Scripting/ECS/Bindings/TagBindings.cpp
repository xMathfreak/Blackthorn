#include "Scripting/ECS/BuiltinBindings.h"

#include "ECS/Components/Tag.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"

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