#pragma once

#include <string>

#include "ECS/Entity.h"

namespace Blackthorn {

namespace Scene {
	class IClientScene;
} // namespace Scene

namespace ECS {
	class World;
} // namespace ECS

namespace Editor::State {

struct Context {
	std::string projectName = "";
	Scene::IClientScene* activeScene = nullptr;
	ECS::World* activeWorld = nullptr;
	ECS::Entity selectedEntity = ECS::INVALID_ENTITY;
};

} // namespace Editor::State

} // namespace Blackthorn