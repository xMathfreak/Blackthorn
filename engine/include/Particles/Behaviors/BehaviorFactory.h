#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Core/Export.h"
#include "Particles/IBehavior.h"

namespace Blackthorn::Particles {

/**
 * @brief Registry mapping a JSON "type" string to a factory function
 * constructing the matching IBehavior from its JSON parameters.
 *
 * Populated once at startup by registerBuiltins() (gravity/drag/wind);
 * gameplay code can register additional behavior types the same way,
 * before any ParticleEffect JSON referencing them is loaded.
 *
 * Used exclusively by the data-driven ParticleEffect loader
 * (Assets/Loaders/ParticleEffectLoader.h) to construct EmitterConfig::behaviors
 * from a JSON array.
 *
 * Not required for behaviors constructed directly in
 * C++, which just push_back a std::unique_ptr<IBehavior> as before.
 */
class BLACKTHORN_API BehaviorFactory {
public:
	using FactoryFn = std::function<std::unique_ptr<IBehavior>(const nlohmann::json&)>;

	static BehaviorFactory& instance();

	/**
	 * @brief Registers a factory function under a JSON "type" string.
	 * Re-registering the same name overwrites the previous entry.
	 */
	void registerType(std::string typeName, FactoryFn factory);

	/**
	 * @brief Constructs a behavior from a single JSON object, using its
	 * "type" field to look up the registered factory.
	 * @return nullptr (with a logged error) if "type" is missing or unrecognized.
	 */
	[[nodiscard]] std::unique_ptr<IBehavior> create(const nlohmann::json& behaviorJson) const;

	/**
	 * @brief Registers factories for the engine's built-in behaviors
	 * ("gravity", "drag", "wind"). Call once during engine
	 * startup before loading any ParticleEffect JSON.
	 */
	void registerBuiltins();

private:
	BehaviorFactory() = default;

	std::unordered_map<std::string, FactoryFn> factories;
};

} // namespace Blackthorn::Particles
