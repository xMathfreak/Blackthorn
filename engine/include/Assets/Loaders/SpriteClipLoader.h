#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "Animation/SpriteClip.h"
#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
#endif

namespace Blackthorn::Animation {

/**
 * @brief Identifies a `.btclip` asset by its pack ID, for use with
 * @c AsyncSpriteClipLoader under @c BT_PACK_MODE.
 */
struct BLACKTHORN_API PackSpriteClipParams final : Assets::LoadParams {
	std::string assetID;

	explicit PackSpriteClipParams(std::string id)
		: assetID(std::move(id))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackSpriteClipParams>(*this);
	}
};

/**
 * @brief Raw, not-yet-parsed bytes of a `.btclip` file, produced by
 * @c AsyncSpriteClipLoader::loadRaw on a worker thread.
 */
struct BLACKTHORN_API RawSpriteClipData : Assets::IRawAssetData {
	std::vector<U8> bytes;

	RawSpriteClipData() = default;
};

/**
 * @brief Shared text-format parser used by both @c SpriteClipLoader and
 * @c AsyncSpriteClipLoader, so the format is defined in exactly one place.
 *
 * See @c SpriteClipLoader for the format description.
 */
class SpriteClipParser {
public:
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
			args >> defaultDuration;
		} else if (key == "grid") {
			float originX, originY, frameW, frameH;
			U32 columns, count;
			args >> originX >> originY >> frameW >> frameH >> columns >> count;

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
			args >> frame.sourceRect.x >> frame.sourceRect.y >> frame.sourceRect.w >> frame.sourceRect.h;

			if (!args.eof())
				args >> frame.duration;

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
 * @brief Loads a @c SpriteClip from a plain-text `.btclip` file.
 *
 * The format is deliberately minimal (no JSON dependency is vendored in
 * this engine). Lines starting with `#` and blank lines are ignored.
 * Recognized directives:
 *
 * @code
 * loop: once | loop | pingpong      # default: loop
 * duration: 0.1                     # default per-frame seconds, default: 0.1
 *
 * # Grid shorthand - slices a texture into a run of equal-size frames:
 * grid: originX originY frameW frameH columns count
 *
 * # Explicit frame - appended after any grid frames, for irregular sheets:
 * frame: x y w h [duration]
 * @endcode
 *
 * @par Example
 * @code
 * loop: loop
 * duration: 0.1
 * grid: 0 0 32 32 6 6
 * @endcode
 * Slices a 6-frame run of 32x32 tiles starting at (0,0), 6 columns wide
 * (i.e. a single row), each shown for 0.1s, looping.
 */
class BLACKTHORN_API SpriteClipLoader final : public Assets::IAssetLoader<SpriteClip> {
public:
	std::unique_ptr<SpriteClip> load(const Assets::LoadParams& params) override {
		const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("SpriteClipLoader: expected PathLoadParams");
			return nullptr;
		}

		std::ifstream file(pp->path);
		if (!file.is_open()) {
			BT_ERROR("SpriteClipLoader: cannot open '{}'", pp->path.string());
			return nullptr;
		}

		auto clip = SpriteClipParser::parse(file);
		if (!clip)
			BT_ERROR("SpriteClipLoader: '{}' produced no frames", pp->path.string());

		return clip;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btclip" };
	}
};

/**
 * @brief Async loader for @c SpriteClip - decodes on a worker thread via
 * @c loadRaw, then parses and installs the asset on the main thread via
 * @c upload, matching the @c AsyncBitmapFontLoader pattern.
 *
 * `.btclip` files are tiny plain text, so "decode" here is just a file
 * read; the actual parse happens in @c upload since it's cheap enough not
 * to need a separate worker-thread step, and keeping parsing on the main
 * thread avoids duplicating @c SpriteClipParser::parse behind two entry
 * points that could drift.
 */
class BLACKTHORN_API AsyncSpriteClipLoader final : public Assets::IAsyncAssetLoader<SpriteClip> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncSpriteClipLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncSpriteClipLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawSpriteClipData&>(rawBase);

		std::istringstream in(std::string(
			reinterpret_cast<const char*>(raw.bytes.data()), raw.bytes.size()
		));

		auto clip = SpriteClipParser::parse(in);
		if (!clip) {
			BT_ERROR("AsyncSpriteClipLoader: '{}' produced no frames", raw.assetID);
			return;
		}

		manager.add<SpriteClip>(raw.assetID, std::move(clip));
		BT_DEBUG("AsyncSpriteClipLoader: '{}' ready", raw.assetID);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btclip" };
	}

private:
#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackSpriteClipParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncSpriteClipLoader: BT_PACK_MODE requires PackSpriteClipParams");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncSpriteClipLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto packed = m_resolver->resolve(pp->assetID);
		if (!packed) {
			BT_ERROR("AsyncSpriteClipLoader: '{}' not found in any mounted pack", pp->assetID);
			return nullptr;
		}

		auto raw = std::make_unique<RawSpriteClipData>();
		raw->bytes = std::move(packed->bytes);
		raw->valid = true;
		return raw;
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncSpriteClipLoader: expected PathLoadParams");
			return nullptr;
		}

		auto raw = std::make_unique<RawSpriteClipData>();
		if (!readFile(pp->path.string(), raw->bytes))
			return nullptr;

		raw->valid = true;
		return raw;
	}

	static bool readFile(const std::string& path, std::vector<U8>& out) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) {
			BT_ERROR("AsyncSpriteClipLoader: cannot open '{}'", path);
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
			BT_ERROR("AsyncSpriteClipLoader: short read from '{}'", path);

		return ok;
	}
};

} // namespace Blackthorn::Animation
