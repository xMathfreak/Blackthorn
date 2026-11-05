#include "Scripting/ECS/BuiltinBindings.hpp"

#include "ECS/Components/Name.hpp"
#include "Scripting/ECS/ComponentBindings.hpp"
#include "Scripting/ECS/ScriptWorld.hpp"

namespace Blackthorn::Scripting::ECS {

void registerBuiltinComponents(ComponentBindings& bindings) {
	Bindings::registerTransformBindings(bindings);
	Bindings::registerKinematicsBindings(bindings);
	Bindings::registerTagBindings(bindings);
	Bindings::registerNameBindings(bindings);
}

} // namespace Blackthorn::Scripting::ECS::Bindings