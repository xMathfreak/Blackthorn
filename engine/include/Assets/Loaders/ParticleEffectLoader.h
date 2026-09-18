#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Animation/SpriteClip.h"
#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Texture.h"
#include "Math/NumericRange.h"
#include "Particles/Behaviors/BehaviorFactory.h"
#include "Particles/EmitterConfig.h"
#include "Particles/ParticleEffect.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
#endif

namespace Blackthorn::Particles {

/**
 * @brief Identifies a `.btfx` asset by its pack ID, for use with
 * AsyncParticleEffectLoader under BT_PACK_MODE.
 */
struct BLACKTHORN_API PackParticleEffectParams final : Assets::LoadParams {
	std::string assetID;

	explicit PackParticleEffectParams(std::string id)
		: assetID(std::move(id))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackParticleEffectParams>(*this);
	}
};

/**
 * @brief Raw, not-yet-parsed bytes of a `.btfx` file, produced by
 * AsyncParticleEffectLoader::loadRaw on a worker thread.
 */
struct BLACKTHORN_API RawParticleEffectData : Assets::IRawAssetData {
	std::vector<U8> bytes;

	RawParticleEffectData() = default;
};

/**
 * @brief Shared JSON parser used by both ParticleEffectLoader and
 * AsyncParticleEffectLoader, so the format is defined in exactly one place.
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
 *       "position": [0.0, 0.0],                    // emitter-local offset, relative to wherever this effect is spawned in the world. default [0, 0]
 *       "maxParticles": 0,                         // 0 = auto-derive, see EmitterConfig::resolveCapacity(). Default: 0
 *       "texture": "assets/particles/spark.png",   // optional
 *       "clip": "assets/particles/spark.btclip",   // optional
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
 * "shape" sub-fields by type:
 * - line -> "length";
 * - box/rectangle -> "size": [w, h];
 * - circle -> "radius".
 * - point has none.
 *
 * @note "texture"/"clip" resolution differs by build mode:
 * - Loose-file builds treat the string as a filesystem path and derive the
 *   asset id from its filename stem using AssetManager::load<T>(path)'s own
 *   convention then synchronously load it if not already resident. This
 *   is a cheap has<T>() lookup in the common case where the asset was
 *   already loaded elsewhere (e.g. scene/level preloading); it only pays a
 *   real synchronous load as a fallback if it wasn't.
 * - BT_PACK_MODE treats the string directly as the pack asset id (paths
 *   aren't meaningful once packed), loaded via PackLoadParams/
 *   PackSpriteClipParams. The packer pipeline is expected to keep this id
 *   stable (e.g. filename stem) so it matches whatever id other assets
 *   packed from the same source file were given.
 */
class ParticleEffectParser {
public:
	static std::unique_ptr<ParticleEffect> parse(const nlohmann::json& root, Assets::AssetManager& assetManager, const std::string& filename) {
		const auto emittersIt = root.find("emitters");
		if (emittersIt == root.end() || !emittersIt->is_array()) {
			BT_ERROR("ParticleEffectParser: missing 'emitters' array");
			return nullptr;
		}

		std::vector<EmitterConfig> emitters;
		emitters.reserve(emittersIt->size());

		for (const auto& emitterJson : *emittersIt)
			emitters.push_back(parseEmitter(emitterJson, assetManager, filename));

		return std::make_unique<ParticleEffect>(std::move(emitters));
	}

private:
	static EmitterConfig parseEmitter(const nlohmann::json& j, Assets::AssetManager& assetManager, const std::string& name) {
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
			config.texture = resolveTexture(j.at("texture").get<std::string>(), assetManager);

		if (j.contains("clip")) {
			if (!config.texture)
				BT_WARN("ParticleEffectParser: emitter in '{}' has a clip set but no texture", name);

			config.clip = resolveClip(j.at("clip").get<std::string>(), assetManager);
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

	static Graphics::Texture* resolveTexture(const std::string& ref, Assets::AssetManager& assetManager) {
#ifdef BT_PACK_MODE
		auto handle = assetManager.load<Graphics::Texture>(ref, Assets::PackLoadParams(ref));
#else
		auto handle = assetManager.load<Graphics::Texture>(std::filesystem::path(ref));
#endif
		if (!handle) {
			BT_ERROR("ParticleEffectParser: failed to resolve texture '{}'", ref);
			return nullptr;
		}

		return handle.get();
	}

	static const Animation::SpriteClip* resolveClip(const std::string& ref, Assets::AssetManager& assetManager) {
#ifdef BT_PACK_MODE
		auto handle = assetManager.load<Animation::SpriteClip>(ref, Animation::PackSpriteClipParams(ref));
#else
		auto handle = assetManager.load<Animation::SpriteClip>(std::filesystem::path(ref));
#endif
		if (!handle) {
			BT_ERROR("ParticleEffectParser: failed to resolve clip '{}'", ref);
			return nullptr;
		}

		return handle.get();
	}
};

/**
 * @brief Loads a ParticleEffect from a `.btfx` JSON file.
 *
 * Nested texture/clip references are resolved synchronously against
 * @c assetManager.
 *
 * @see ParticleEffectParser
 */
class BLACKTHORN_API ParticleEffectLoader final : public Assets::IAssetLoader<ParticleEffect> {
public:
	/**
	 * @param assetManager Manager used to resolve nested texture/clip
	 * references.
	 */
	explicit ParticleEffectLoader(Assets::AssetManager& am)
		: assetManager(am)
	{}

	std::unique_ptr<ParticleEffect> load(const Assets::LoadParams& params) override {
		const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("ParticleEffectLoader: expected PathLoadParams");
			return nullptr;
		}

		std::ifstream file(pp->path);
		if (!file.is_open()) {
			BT_ERROR("ParticleEffectLoader: cannot open '{}'", pp->path.string());
			return nullptr;
		}

		nlohmann::json root;
		try {
			file >> root;
		} catch (const nlohmann::json::parse_error& e) {
			BT_ERROR("ParticleEffectLoader: failed to parse '{}': {}", pp->path.string(), e.what());
			return nullptr;
		}

		auto effect = ParticleEffectParser::parse(root, assetManager, pp->path.string());
		if (!effect)
			BT_ERROR("ParticleEffectLoader: '{}' produced no emitters", pp->path.string());

		return effect;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btfx" };
	}

private:
	Assets::AssetManager& assetManager;
};

/**
 * @brief Async loader for ParticleEffect.
 *
 * Decodes on a worker thread via loadRaw (just a file/pack read. `.btfx`
 * is plain JSON text, cheap to parse), then parses and resolves nested
 * texture/clip references on the main thread via upload, matching the
 * AsyncSpriteClipLoader pattern. Nested resolution uses AssetManager's
 * synchronous load() even here, since upload() already runs on the main
 * thread as the second stage of the async pipeline.
 *
 * @see ParticleEffectParser
 */
class BLACKTHORN_API AsyncParticleEffectLoader final : public Assets::IAsyncAssetLoader<ParticleEffect> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncParticleEffectLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncParticleEffectLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawParticleEffectData&>(rawBase);

		nlohmann::json root;
		try {
			root = nlohmann::json::parse(std::string(
				reinterpret_cast<const char*>(raw.bytes.data()), raw.bytes.size()
			));
		} catch (const nlohmann::json::parse_error& e) {
			BT_ERROR("AsyncParticleEffectLoader: failed to parse '{}': {}", raw.assetID, e.what());
			return;
		}

		auto effect = ParticleEffectParser::parse(root, manager, raw.assetID);
		if (!effect) {
			BT_ERROR("AsyncParticleEffectLoader: '{}' produced no emitters", raw.assetID);
			return;
		}

		manager.add<ParticleEffect>(raw.assetID, std::move(effect));
		BT_DEBUG("AsyncParticleEffectLoader: '{}' ready", raw.assetID);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btfx" };
	}

private:
#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackParticleEffectParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncParticleEffectLoader: BT_PACK_MODE requires PackParticleEffectParams");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncParticleEffectLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto packed = m_resolver->resolve(pp->assetID);
		if (!packed) {
			BT_ERROR("AsyncParticleEffectLoader: '{}' not found in any mounted pack", pp->assetID);
			return nullptr;
		}

		auto raw = std::make_unique<RawParticleEffectData>();
		raw->bytes = std::move(packed->bytes);
		raw->valid = true;
		return raw;
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncParticleEffectLoader: expected PathLoadParams");
			return nullptr;
		}

		auto raw = std::make_unique<RawParticleEffectData>();
		if (!readFile(pp->path.string(), raw->bytes))
			return nullptr;

		raw->valid = true;
		return raw;
	}

	static bool readFile(const std::string& path, std::vector<U8>& out) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) {
			BT_ERROR("AsyncParticleEffectLoader: cannot open '{}'", path);
			return false;
		}

		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);

		if (size < 0) {
			std::fclose(f);
			return false;
		}

		out.resize(static_cast<size_t>(size));
		const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
		std::fclose(f);

		if (!ok)
			BT_ERROR("AsyncParticleEffectLoader: short read from '{}'", path);

		return ok;
	}
};

} // namespace Blackthorn::Particles
