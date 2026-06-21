#pragma once

#include <filesystem>
#include <string>

#include "Assets/AssetDirectoryCache.h"
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
	std::filesystem::path assetsRoot = "assets";

	Scene::IClientScene* activeScene = nullptr;
	ECS::World* activeWorld = nullptr;
	ECS::Entity selectedEntity = ECS::INVALID_ENTITY;

	Assets::AssetDirectoryCache assetCache;
	bool importRequested = false;
};

} // namespace Editor::State

} // namespace Blackthorn