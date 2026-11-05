#include "Blackthorn/Scripting/ECS/BuiltinBindings.hpp"

#include "Blackthorn/ECS/Components/Name.hpp"
#include "Blackthorn/Scripting/ECS/ComponentBindings.hpp"
#include "Blackthorn/Scripting/ECS/ScriptWorld.hpp"

namespace Blackthorn::Scripting::ECS {

void registerBuiltinComponents(ComponentBindings& bindings) {
	Bindings::registerTransformBindings(bindings);
	Bindings::registerKinematicsBindings(bindings);
	Bindings::registerTagBindings(bindings);
	Bindings::registerNameBindings(bindings);
}

} // namespace Blackthorn::Scripting::ECS::Bindings