#include "Scripting/ECS/BuiltinBindings.h"

#include "ECS/Components/Kinematics.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"
#include "Scripting/LuaMath.h"

namespace Blackthorn::Scripting::ECS::Bindings {

void registerKinematicsBindings(Scripting::ECS::ComponentBindings& bindings) {
	bindings.declareComponent<Blackthorn::ECS::Components::Kinematics>("kinematics");

	bindings.registerGetter<Blackthorn::ECS::Components::Kinematics>(
		"oldPosition",
		[](ScriptWorld& world, EntityHandle handle) {
			return world.requireComponent<Blackthorn::ECS::Components::Kinematics>(handle).oldPosition;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Kinematics>(
		"oldPosition",
		[](ScriptWorld& world, EntityHandle handle, glm::vec2 pos) {
			auto& Kinematics = world.requireComponent<Blackthorn::ECS::Components::Kinematics>(handle);
			Kinematics.oldPosition = pos;
		}
	);

	bindings.registerGetter<Blackthorn::ECS::Components::Kinematics>(
		"acceleration",
		[](ScriptWorld& world, EntityHandle handle) {
			return world.requireComponent<Blackthorn::ECS::Components::Kinematics>(handle).acceleration;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Kinematics>(
		"acceleration",
		[](ScriptWorld& world, EntityHandle handle, glm::vec2 acc) {
			auto& Kinematics = world.requireComponent<Blackthorn::ECS::Components::Kinematics>(handle);
			Kinematics.acceleration = acc;
		}
	);
}

} // namespace Blackthorn::Scripting::ECS::Bindings