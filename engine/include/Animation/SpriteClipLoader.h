#pragma once

#include <cstdio>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Animation/SpriteClip.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"

namespace Blackthorn::Animation {

/**
 * @brief Raw bytes of a `.btclip` file, produced by AsyncSpriteClipLoader::loadRaw.
 */
struct BLACKTHORN_API RawSpriteClipData : Assets::IRawAssetData {
	std::vector<U8> bytes;

	RawSpriteClipData() = default;
};

class SpriteClipParser {
public:
	static std::unique_ptr<SpriteClip> parse(
		std::istream& in,
		std::string_view name
	) {
		const auto json = nlohmann::json::parse(in, nullptr, true, true, true);

		const auto framesIt = json.find("frames");
		if (framesIt == json.end() || !framesIt->is_array()) {
			BT_WARN("SpriteClipParser: '{}' is missing a 'frames' array", name);
			return nullptr;
		}

		auto clip = std::make_unique<SpriteClip>();
		clip->loopMode = parseLoopMode(json.value("loop", "infinite"));

		for (const auto& frame : *framesIt) {
			parseFrame(frame, *clip, name);
		}

		return clip;
	}

private:
	static void parseFrame(
		const nlohmann::json& frame,
		SpriteClip& clip,
		std::string_view name
	) {
		const auto typeIt = frame.find("type");
		if (typeIt == frame.end() || !typeIt->is_string()) {
			BT_WARN("SpriteClipParser: Missing or invalid 'type' in '{}'", name);
			return;
		}

		const auto type = typeIt->get<std::string>();

		if (type == "frame") {
			parseSingleFrame(frame, clip);
		} else if (type == "grid") {
			parseGrid(frame, clip);
		} else {
			BT_WARN("SpriteClipParser: Unknown frame type '{}' in '{}'", type, name);
		}
	}

	static void parseSingleFrame(
		const nlohmann::json& json,
		SpriteClip& clip
	) {
		Frame frame{};

		frame.sourceRect = SDL_FRect{
			json.value("x", 0.0f),
			json.value("y", 0.0f),
			aliasedValue<float>(json, {"w", "width"}, 0.0f),
			aliasedValue<float>(json, {"h", "height"}, 0.0f)
		};

		frame.duration = json.value("duration", 0.0f);
		clip.duration += frame.duration;

		clip.frames.push_back(frame);
	}

	static void parseGrid(
		const nlohmann::json& json,
		SpriteClip& clip
	) {
		const int count = json.value("count", 0);
		const int columns = json.value("columns", 0);

		if (count <= 0) {
			BT_WARN("SpriteClipParser: 'grid' frame count must be greater than 0");
			return;
		}

		if (columns <= 0) {
			BT_WARN("SpriteClipParser: 'grid' columns must be greater than 0");
			return;
		}

		const float originX = json.at("x").get<float>();
		const float originY = json.at("y").get<float>();
		const float frameW = aliasedValue(json, {"w", "width"}, 0.0f);
		const float frameH = aliasedValue(json, {"h", "height"}, 0.0f);
		const float duration = json.value("duration", 0.0f);

		for (int i = 0; i < count; ++i) {
			const auto col = i % columns;
			const auto row = i / columns;

			Frame frame{};

			frame.sourceRect = SDL_FRect{
				originX + static_cast<float>(col) * frameW,
				originY + static_cast<float>(row) * frameH,
				frameW,
				frameH
			};

			frame.duration = duration;
			clip.duration += frame.duration;
			clip.frames.push_back(frame);
		}
	}

	static LoopMode parseLoopMode(std::string_view mode) {
		if (mode == "once")
			return LoopMode::Once;

		if (mode == "ping-pong" || mode == "pingpong")
			return LoopMode::PingPong;

		return LoopMode::Loop;
	}

	template <typename T>
	static T aliasedValue(
		const nlohmann::json& json,
		std::initializer_list<std::string_view> aliases,
		T defaultValue
	) {
		for (const auto alias : aliases) {
			if (json.contains(alias)) {
				return json.at(alias).get<T>();
			}
		}

		return defaultValue;
	}
};


/**
 * @brief Loads a SpriteClip from a plain-text `.btclip` file through the resolver.
 *
 * A clip has no texture reference. the emitter or sprite that uses it supplies
 * the texture.
 *
 * @par Example
 * @code
 * {
 *   "loop": "infinite",
 *   "frames": [
 *     {
 *       "type": "grid",
 *       "x": 0,
 *       "y": 0,
 *       "width": 32,
 *       "height": 32,
 *       "columns": 6,
 *       "count": 6,
 *       "duration": 0.5
 *     },
 *     {
 *       "type": "frame",
 *       "x": 64,
 *       "y": 32,
 *       "width": 64,
 *       "height": 32
 *     }
 *   ]
 * }
 * @endcode
 */
class BLACKTHORN_API SpriteClipLoader final : public Assets::IAssetLoader<SpriteClip> {
public:
	std::unique_ptr<SpriteClip> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("SpriteClipLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("SpriteClipLoader: unrecognized LoadParams type");
			return nullptr;
		}

		const auto id = Blackthorn::normalizeAssetPath(pp->source.generic_string());
		if (!id) {
			BT_ERROR("SpriteClipLoader: invalid asset path '{}'", pp->source.generic_string());
			return nullptr;
		}

		const auto bytes = resolver->resolve(*id);
		if (!bytes)
			return nullptr;

		std::istringstream in(std::string(reinterpret_cast<const char*>(bytes->bytes.data()), bytes->bytes.size()));

		auto clip = SpriteClipParser::parse(in, *id);
		if (!clip)
			BT_ERROR("SpriteClipLoader: '{}' produced no frames", *id);

		return clip;
	}
};

/**
 * @brief Async sprite clip loader.
 *
 * loadRaw reads the bytes through the resolver on a worker thread. The parse
 * runs in upload on the main thread, since a `.btclip` is small and keeping
 * the parser in one place avoids drift between two entry points.
 */
class BLACKTHORN_API AsyncSpriteClipLoader final : public Assets::IAsyncAssetLoader<SpriteClip> {
public:
	AsyncSpriteClipLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncSpriteClipLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params);
		if (!pp)
			return nullptr;

		const auto id = Blackthorn::normalizeAssetPath(pp->source.generic_string());
		if (!id)
			return nullptr;

		auto bytes = resolver->resolve(*id);
		if (!bytes)
			return nullptr;

		auto raw = std::make_unique<RawSpriteClipData>();
		raw->bytes = std::move(bytes->bytes);
		raw->valid = true;
		raw->displayName = *id;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawSpriteClipData&>(rawBase);

		std::istringstream in(std::string(reinterpret_cast<const char*>(raw.bytes.data()), raw.bytes.size()));

		auto clip = SpriteClipParser::parse(in, raw.displayName);
		if (!clip) {
			BT_ERROR("AsyncSpriteClipLoader: '{}' produced no frames", raw.displayName);
			return;
		}

		manager.add<SpriteClip>({raw.assetName}, std::move(clip));
		BT_DEBUG("AsyncSpriteClipLoader: '{}' ready", raw.displayName);
	}
};

} // namespace Blackthorn::Animation
