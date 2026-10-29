#include "Scripting/ECS/BuiltinBindings.h"

#include "ECS/Components/Name.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"

namespace Blackthorn::Scripting::ECS::Bindings {

void registerNameBindings(Scripting::ECS::ComponentBindings& bindings) {
	bindings.declareComponent<Blackthorn::ECS::Components::Name>("name");

	bindings.registerGetter<Blackthorn::ECS::Components::Name>(
		"value",
		[](ScriptWorld& w, EntityHandle h) {
			return w.requireComponent<Blackthorn::ECS::Components::Name>(h).value;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Name>("value",
	[](ScriptWorld& w, EntityHandle h, std::string value) {
		if (!value.empty()) {
			const auto owner = w.findEntityNamed(value);
			if (owner && owner->value != h.value)
				throw std::runtime_error("name already in use: " + value);
		}

		w.requireComponent<Blackthorn::ECS::Components::Name>(h).value = std::move(value);
	});
}

} // namespace Blackthorn::Scripting::ECS::Bindings