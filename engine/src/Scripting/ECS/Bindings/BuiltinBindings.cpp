#include "Scripting/ECS/BuiltinBindings.h"

#include "ECS/Components/Name.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"

namespace Blackthorn::Scripting::ECS {

void registerBuiltinComponents(ComponentBindings& bindings) {
	Bindings::registerTransformBindings(bindings);
	Bindings::registerKinematicsBindings(bindings);
	Bindings::registerTagBindings(bindings);
	Bindings::registerNameBindings(bindings);
}

} // namespace Blackthorn::Scripting::ECS::Bindings