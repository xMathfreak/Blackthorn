#pragma once

#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "Animation/SpriteClip.h"
#include "Animation/SpriteClipLoader.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Texture.h"
#include "Math/NumericRange.h"
#include "Particles/Behaviors/BehaviorFactory.h"
#include "Particles/EmitterConfig.h"
#include "Particles/ParticleEffect.h"

namespace Blackthorn::Particles {

/**
 * @brief Raw bytes of a `.btfx` file plus its canonical ID.
 *
 * baseID is what nested references resolve against. It's the normalized
 * path the effect was requested under, not the caller's load ID.
 */
struct BLACKTHORN_API RawParticleEffectData : Assets::IRawAssetData {
	std::vector<U8> bytes;
	std::string     baseID;

	RawParticleEffectData() = default;
};

/**
 * @brief Shared JSON parser used by both particle effect loaders.
 *
 * @par Format
 * @code
 * {
 *   "emitters": [
 *     {
 *       "distribution": "random",                  // "random" | "boundary". Default: "random"
 *       "shape": { "type": "circle", "radius": 2 },// "point" | "line" | "box"/"rectangle" | "circle". Default: "point"
 *       "rate": 50.0,                              // particles/second. Default: 10
 *       "lifetime": [0.5, 1.5],                    // [min, max] seconds, or a single number. Default: [1, 10]
 *       "speed": [2.0, 6.0],                       // [min, max] units/second, or a single number. Default: 1
 *       "size": [40, 80],                          // [min, max] pixels, or a single number. Default: 50
 *       "rotation": 0.0,                           // [min, max] radians. Default: [0, 2 * PI]
 *       "offset": [0.0, 0.0],                      // emitter-local offset from the spawn point. Default: [0, 0]
 *       "maxParticles": 0,                         // 0 = auto-derive. Default: 0
 *       "texture": "smoke.png",                    // optional, relative to this file
 *       "clip": "spark1.btclip",                   // optional, relative to this file
 *       "behaviors": [                             // optional, see BehaviorFactory
 *         { "type": "gravity", "acceleration": [0, 9.81] },
 *         { "type": "drag", "coefficient": 0.5 },
 *         { "type": "wind", "direction": [1, 0], "strength": 2.0, "gustiness": 0.3 }
 *       ]
 *     }
 *   ]
 * }
 * @endcode
 *
 * @note References are resolved with Assets::AssetResolver::resolveReference
 * against the effect's canonical ID, so the same file works in loose and
 * pack builds, under any root name. See AssetReference.h for the grammar.
 */
class ParticleEffectParser {
public:
	/**
	 * @param root    Parsed JSON document.
	 * @param manager Used to load nested textures and clips, and for resolution.
	 * @param baseID  Canonical ID of the `.btfx` file, for resolving references.
	 */
	static std::unique_ptr<ParticleEffect> parse(
		const nlohmann::json& root,
		Assets::AssetManager& manager,
		const std::string& baseID
	) {
		const auto emittersIt = root.find("emitters");
		if (emittersIt == root.end() || !emittersIt->is_array()) {
			BT_ERROR("ParticleEffectParser: '{}' is missing an 'emitters' array", baseID);
			return nullptr;
		}

		std::vector<EmitterConfig> emitters;
		emitters.reserve(emittersIt->size());

		for (const auto& emitterJson : *emittersIt)
			emitters.push_back(parseEmitter(emitterJson, manager, baseID));

		return std::make_unique<ParticleEffect>(std::move(emitters));
	}

private:
	static EmitterConfig parseEmitter(
		const nlohmann::json& j,
		Assets::AssetManager& manager,
		const std::string& baseID
	) {
		EmitterConfig config;

		config.distribution = j.value("distribution", std::string("random")) == "boundary"
			? Distribution::Boundary
			: Distribution::Random;

		config.shape = parseShape(j.value("shape", nlohmann::json::object()));
		config.rate = j.value("rate", 10.0f);
		config.lifetimeRange = parseRange(j, "lifetime", Math::FloatRange{1.0f, 10.0f});
		config.speedRange = parseRange(j, "speed", Math::FloatRange{1.0f});
		config.sizeRange = parseRange(j, "size", Math::FloatRange{50.0f});
		config.rotation = parseRange(j, "rotation", Math::FloatRange{0.0f, 2 * std::numbers::pi_v<float>});
		config.offset = readVec2(j, "offset", {0.0f, 0.0f});
		config.maxParticles = j.value("maxParticles", 0u);

		if (j.contains("texture"))
			config.texture = resolveTexture(j.at("texture").get<std::string>(), manager, baseID);

		if (j.contains("clip")) {
			if (!config.texture)
				BT_WARN("ParticleEffectParser: emitter in '{}' has a clip set but no texture", baseID);

			config.clip = resolveClip(j.at("clip").get<std::string>(), manager, baseID);
		}

		if (const auto behaviorsIt = j.find("behaviors"); behaviorsIt != j.end() && behaviorsIt->is_array()) {
			for (const auto& behaviorJson : *behaviorsIt) {
				if (auto behavior = BehaviorFactory::instance().create(behaviorJson))
					config.behaviors.push_back(std::move(behavior));
			}
		}

		return config;
	}

	static EmitterShape parseShape(const nlohmann::json& j) {
		const std::string type = j.value("type", std::string("point"));

		if (type == "line")
			return Line{ j.value("length", 1.0f) };

		if (type == "box" || type == "rectangle")
			return Box{ readVec2(j, "size", {1.0f, 1.0f}) };

		if (type == "circle")
			return Circle{ j.value("radius", 1.0f) };

		return Point{};
	}

	static Math::FloatRange parseRange(const nlohmann::json& j, const char* key, Math::FloatRange fallback) {
		const auto it = j.find(key);
		if (it == j.end())
			return fallback;

		if (it->is_array() && it->size() == 2)
			return Math::FloatRange{ it->at(0).get<float>(), it->at(1).get<float>() };

		if (it->is_number())
			return Math::FloatRange{ it->get<float>() };

		return fallback;
	}

	static glm::vec2 readVec2(const nlohmann::json& j, const char* key, glm::vec2 fallback) {
		const auto it = j.find(key);
		if (it == j.end() || !it->is_array() || it->size() != 2)
			return fallback;

		return { it->at(0).get<float>(), it->at(1).get<float>() };
	}

	static Graphics::Texture* resolveTexture(
		const std::string& ref,
		Assets::AssetManager& manager,
		const std::string& baseID
	) {
		const auto id = manager.resolver().resolveReference(baseID, ref);
		if (!id) {
			BT_ERROR("ParticleEffectParser: invalid texture reference '{}' in '{}'", ref, baseID);
			return nullptr;
		}

		auto handle = manager.load<Graphics::Texture>(*id);
		if (!handle) {
			BT_ERROR("ParticleEffectParser: failed to load texture '{}' (from '{}')", *id, baseID);
			return nullptr;
		}

		return handle.get();
	}

	static const Animation::SpriteClip* resolveClip(
		const std::string& ref,
		Assets::AssetManager& manager,
		const std::string& baseID
	) {
		const auto id = manager.resolver().resolveReference(baseID, ref);
		if (!id) {
			BT_ERROR("ParticleEffectParser: invalid clip reference '{}' in '{}'", ref, baseID);
			return nullptr;
		}

		auto handle = manager.load<Animation::SpriteClip>(*id);
		if (!handle) {
			BT_ERROR("ParticleEffectParser: failed to load clip '{}' (from '{}')", *id, baseID);
			return nullptr;
		}

		return handle.get();
	}
};

/**
 * @brief Loads a ParticleEffect synchronously from the resolver.
 *
 * Works in loose and pack builds alike. The load ID must be a canonical path,
 * for example "assets/particles/spark.btfx".
 */
class BLACKTHORN_API ParticleEffectLoader final : public Assets::IAssetLoader<ParticleEffect> {
public:
	explicit ParticleEffectLoader(Assets::AssetManager& am)
		: assetManager(am)
	{}

	std::unique_ptr<ParticleEffect> load(const Assets::LoadParams& params) override {
		const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("ParticleEffectLoader: unrecognized LoadParams type");
			return nullptr;
		}

		const auto id = Blackthorn::normalizeAssetPath(pp->source.generic_string());
		if (!id) {
			BT_ERROR("ParticleEffectLoader: invalid asset path '{}'", pp->source.generic_string());
			return nullptr;
		}

		const auto bytes = assetManager.resolver().resolve(*id);
		if (!bytes)
			return nullptr;

		const std::string text(reinterpret_cast<const char*>(bytes->bytes.data()), bytes->bytes.size());

		nlohmann::json root;
		try {
			root = nlohmann::json::parse(text);
		} catch (const nlohmann::json::parse_error& e) {
			BT_ERROR("ParticleEffectLoader: failed to parse '{}': {}", *id, e.what());
			return nullptr;
		}

		return ParticleEffectParser::parse(root, assetManager, *id);
	}

private:
	Assets::AssetManager& assetManager;
};

/**
 * @brief Asynchronous loader for ParticleEffect.
 *
 * loadRaw reads the bytes through the resolver on a worker thread. upload parses
 * the JSON and resolves nested references on the main thread.
 */
class BLACKTHORN_API AsyncParticleEffectLoader final : public Assets::IAsyncAssetLoader<ParticleEffect> {
public:
	AsyncParticleEffectLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params);
		if (!pp)
			return nullptr;

		const auto id = Blackthorn::normalizeAssetPath(pp->source.generic_string());
		if (!id)
			return nullptr;

		if (!resolver) {
			BT_ERROR("AsyncParticleEffectLoader: no resolver, loader was not registered");
			return nullptr;
		}

		auto bytes = resolver->resolve(*id);
		if (!bytes)
			return nullptr;

		auto raw = std::make_unique<RawParticleEffectData>();
		raw->bytes = std::move(bytes->bytes);
		raw->baseID = *id;
		raw->valid = true;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawParticleEffectData&>(rawBase);

		const std::string text(reinterpret_cast<const char*>(raw.bytes.data()), raw.bytes.size());

		nlohmann::json root;
		try {
			root = nlohmann::json::parse(text);
		} catch (const nlohmann::json::parse_error& e) {
			BT_ERROR("AsyncParticleEffectLoader: failed to parse '{}': {}", raw.baseID, e.what());
			return;
		}

		auto effect = ParticleEffectParser::parse(root, manager, raw.baseID);
		if (!effect) {
			BT_ERROR("AsyncParticleEffectLoader: '{}' produced no effect", raw.baseID);
			return;
		}

		manager.add<ParticleEffect>(raw.assetID, std::move(effect));
		BT_DEBUG("AsyncParticleEffectLoader: '{}' ready", raw.assetID);
	}

};

} // namespace Blackthorn::Particles
