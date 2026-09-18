#include "Particles/Behaviors/BehaviorFactory.h"

#include "Debug/Logger.h"
#include "Particles/Behaviors/Drag.h"
#include "Particles/Behaviors/Gravity.h"
#include "Particles/Behaviors/Wind.h"

namespace Blackthorn::Particles {

namespace {

glm::vec2 readVec2(const nlohmann::json& j, const char* key, glm::vec2 fallback) {
	if (!j.contains(key))
		return fallback;

	const auto& arr = j.at(key);
	if (!arr.is_array() || arr.size() != 2)
		return fallback;

	return { arr[0].get<float>(), arr[1].get<float>() };
}

} // namespace

BehaviorFactory& BehaviorFactory::instance() {
	static BehaviorFactory factory;
	return factory;
}

void BehaviorFactory::registerType(std::string typeName, FactoryFn factory) {
	factories[std::move(typeName)] = std::move(factory);
}

std::unique_ptr<IBehavior> BehaviorFactory::create(const nlohmann::json& behaviorJson) const {
	const auto typeIt = behaviorJson.find("type");
	if (typeIt == behaviorJson.end() || !typeIt->is_string()) {
		BT_ERROR("BehaviorFactory: behavior entry missing string 'type'");
		return nullptr;
	}

	const std::string type = typeIt->get<std::string>();

	const auto it = factories.find(type);
	if (it == factories.end()) {
		BT_ERROR("BehaviorFactory: no behavior registered for type '{}'", type);
		return nullptr;
	}

	return it->second(behaviorJson);
}

void BehaviorFactory::registerBuiltins() {
	registerType("gravity", [](const nlohmann::json& j) -> std::unique_ptr<IBehavior> {
		return std::make_unique<Gravity>(readVec2(j, "acceleration", {0.0f, 9.81f}));
	});

	registerType("drag", [](const nlohmann::json& j) -> std::unique_ptr<IBehavior> {
		return std::make_unique<Drag>(j.value("coefficient", 1.0f));
	});

	registerType("wind", [](const nlohmann::json& j) -> std::unique_ptr<IBehavior> {
		return std::make_unique<Wind>(
			readVec2(j, "direction", {1.0f, 0.0f}),
			j.value("strength", 1.0f),
			j.value("gustiness", 0.0f),
			j.value("noiseScale", 0.5f),
			j.value("gustFrequency", 1.0f)
		);
	});
}

} // namespace Blackthorn::Particles
