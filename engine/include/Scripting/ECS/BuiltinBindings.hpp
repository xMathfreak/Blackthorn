#pragma once

#include "Core/Export.h"

namespace Blackthorn::Scripting::ECS {

class ComponentBindings;

namespace Bindings {


void registerTransformBindings(ComponentBindings& bindings);
void registerSpriteBindings(ComponentBindings& bindings);
void registerNameBindings(ComponentBindings& bindings);
void registerTagBindings(ComponentBindings& bindings);
void registerKinematicsBindings(ComponentBindings& bindings);
void registerPersistentBindings(ComponentBindings& bindings);
void registerAnimationBindings(ComponentBindings& bindings);

} // namespace Bindings

void BLACKTHORN_API registerBuiltinComponents(ComponentBindings& bindings);

} // namespace Blackthorn::Scripting::ECS::Bindings