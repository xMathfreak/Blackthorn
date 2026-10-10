#pragma once

#include <cstdio>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

/**
 * @brief Text-format parser shared by both sprite clip loaders.
 *
 * Lines are applied in file order. Blank lines and lines starting with '#' are
 * ignored, and CRLF line endings are accepted.
 */
class SpriteClipParser {
public:
	/**
	 * @return The parsed clip, or nullptr if it contains no frames.
	 */
	static std::unique_ptr<SpriteClip> parse(std::istream& in) {
		auto clip = std::make_unique<SpriteClip>();
		float defaultDuration = 0.1f;

		std::string line;
		while (std::getline(in, line))
			parseLine(line, *clip, defaultDuration);

		return clip->isValid() ? std::move(clip) : nullptr;
	}

private:
	static void parseLine(const std::string& rawLine, SpriteClip& clip, float& defaultDuration) {
		const std::string line = trim(rawLine);
		if (line.empty() || line[0] == '#')
			return;

		const size_t colon = line.find(':');
		if (colon == std::string::npos)
			return;

		const std::string key = trim(line.substr(0, colon));
		std::istringstream args(line.substr(colon + 1));

		if (key == "loop") {
			std::string mode;
			args >> mode;
			clip.loopMode = parseLoopMode(mode);
		} else if (key == "duration") {
			float value = 0.0f;
			if (args >> value)
				defaultDuration = value;
			else
				BT_ERROR("SpriteClipParser: 'duration' needs a number, got '{}'", trim(line.substr(colon + 1)));
		} else if (key == "grid") {
			float originX = 0.0f, originY = 0.0f, frameW = 0.0f, frameH = 0.0f;
			U32 columns = 0, count = 0;

			if (!(args >> originX >> originY >> frameW >> frameH >> columns >> count)) {
				BT_ERROR("SpriteClipParser: malformed 'grid' line '{}'", line);
				return;
			}

			if (columns == 0) {
				BT_ERROR("SpriteClipParser: 'grid' columns must be non-zero");
				return;
			}

			for (U32 i = 0; i < count; ++i) {
				const U32 col = i % columns;
				const U32 row = i / columns;

				Frame frame;
				frame.sourceRect = SDL_FRect{
					originX + static_cast<float>(col) * frameW,
					originY + static_cast<float>(row) * frameH,
					frameW,
					frameH
				};
				frame.duration = defaultDuration;

				clip.frames.push_back(frame);
			}
		} else if (key == "frame") {
			Frame frame;
			frame.duration = defaultDuration;

			if (!(args >> frame.sourceRect.x >> frame.sourceRect.y >> frame.sourceRect.w >> frame.sourceRect.h)) {
				BT_ERROR("SpriteClipParser: malformed 'frame' line '{}'", line);
				return;
			}

			float duration = 0.0f;
			if (args >> duration)
				frame.duration = duration;

			clip.frames.push_back(frame);
		}
	}

	static LoopMode parseLoopMode(const std::string& mode) {
		if (mode == "once")
			return LoopMode::Once;
		if (mode == "pingpong")
			return LoopMode::PingPong;

		return LoopMode::Loop;
	}

	static std::string trim(const std::string& s) {
		const size_t start = s.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			return "";

		const size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}
};

/**
 * @brief Loads a SpriteClip from a plain-text `.btclip` file through the resolver.
 *
 * @code
 * loop: once | loop | pingpong      # default: loop
 * duration: 0.1                     # default per-frame duration, in seconds
 *
 * # Grid shorthand: append `count` equal-sized frames, left to right, top to bottom.
 * grid: originX originY frameW frameH columns count
 *
 * # Single frame: x y w h [duration]
 * frame: x y w h [duration]
 * @endcode
 *
 * Directives are applied in file order, so a `duration:` line affects only the
 * grid and frame lines that follow it. A clip has no texture reference: the
 * emitter or sprite that uses it supplies the texture.
 *
 * @par Example
 * @code
 * loop: loop
 * duration: 0.1
 * grid: 0 0 32 32 6 6
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

		auto clip = SpriteClipParser::parse(in);
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
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawSpriteClipData&>(rawBase);

		std::istringstream in(std::string(reinterpret_cast<const char*>(raw.bytes.data()), raw.bytes.size()));

		auto clip = SpriteClipParser::parse(in);
		if (!clip) {
			BT_ERROR("AsyncSpriteClipLoader: '{}' produced no frames", raw.assetID);
			return;
		}

		manager.add<SpriteClip>(raw.assetID, std::move(clip));
		BT_DEBUG("AsyncSpriteClipLoader: '{}' ready", raw.assetID);
	}
};

} // namespace Blackthorn::Animation
