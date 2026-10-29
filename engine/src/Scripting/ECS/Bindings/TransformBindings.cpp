#include "Scripting/ECS/BuiltinBindings.h"

#include "ECS/Components/Transform.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"
#include "Scripting/LuaMath.h"

namespace Blackthorn::Scripting::ECS::Bindings {

void registerTransformBindings(ComponentBindings& bindings) {
	bindings.declareComponent<Blackthorn::ECS::Components::Transform>("transform");

	bindings.registerGetter<Blackthorn::ECS::Components::Transform>(
		"position",
		[](ScriptWorld& world, EntityHandle handle) {
			return world.requireComponent<Blackthorn::ECS::Components::Transform>(handle).position;
		}
	);

	bindings.registerGetter<Blackthorn::ECS::Components::Transform>(
		"angle",
		[](ScriptWorld& world, EntityHandle handle) {
			return world.requireComponent<Blackthorn::ECS::Components::Transform>(handle).angle;
		}
	);

	bindings.registerGetter<Blackthorn::ECS::Components::Transform>(
		"scale",
		[](ScriptWorld& world, EntityHandle handle) {
			return world.requireComponent<Blackthorn::ECS::Components::Transform>(handle).scale;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Transform>(
		"position",
		[](ScriptWorld& world, EntityHandle handle, glm::vec2 pos) {
			auto& transform = world.requireComponent<Blackthorn::ECS::Components::Transform>(handle);
			transform.position = pos;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Transform>(
		"angle",
		[](ScriptWorld& world, EntityHandle handle, float angle) {
			auto& transform = world.requireComponent<Blackthorn::ECS::Components::Transform>(handle);
			transform.angle = angle;
		}
	);

	bindings.registerSetter<Blackthorn::ECS::Components::Transform>(
		"scale",
		[](ScriptWorld& world, EntityHandle handle, float scale) {
			auto& transform = world.requireComponent<Blackthorn::ECS::Components::Transform>(handle);
			transform.scale = scale;
		}
	);
}

} // namespace Blackthorn::Scripting::ECS::Bindings