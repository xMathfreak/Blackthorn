#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Fonts/BitmapFont.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
	#include "Assets/PackMount.h"
#endif

namespace Blackthorn::Fonts {

struct BLACKTHORN_API BitmapParams : Assets::LoadParams {
	std::filesystem::path texturePath;
	std::filesystem::path metricsPath;

	BitmapParams(const std::filesystem::path& texture, const std::filesystem::path& metrics)
		: texturePath(texture)
		, metricsPath(metrics)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<BitmapParams>(*this);
	}
};

struct BLACKTHORN_API PackBitmapParams final : Assets::LoadParams {
	std::string bmfID;
	std::string textureID;
	std::string metricsID;

	explicit PackBitmapParams(std::string bmf)
		: bmfID(std::move(bmf))
	{}

	PackBitmapParams(std::string texture, std::string metrics)
		: textureID(std::move(texture))
		, metricsID(std::move(metrics))
	{}

	bool isSingleFile() const { return !bmfID.empty(); }

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackBitmapParams>(*this);
	}
};

struct BLACKTHORN_API RawBitmapFontData : Assets::IRawAssetData {
	std::vector<U8> bmfBytes;

	std::vector<U8> textureBytes;
	std::vector<U8> metricsBytes;

	bool isSingleFile = false;

	RawBitmapFontData() = default;
};

class BLACKTHORN_API BitmapFontLoader final : public Assets::IAssetLoader<BitmapFont> {
public:
	std::unique_ptr<BitmapFont> load(const Assets::LoadParams& params) override {
		auto font = std::make_unique<BitmapFont>();

		if (const auto* bp = dynamic_cast<const BitmapParams*>(&params)) {
			font->loadFromFile(bp->texturePath, bp->metricsPath);
			return font;
		}

		if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			font->loadFromBMFont(pp->path);
			return font;
		}

		return nullptr;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".bmf", ".fnt" };
	}
};

class BLACKTHORN_API AsyncBitmapFontLoader final : public Assets::IAsyncAssetLoader<BitmapFont> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncBitmapFontLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncBitmapFontLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawBitmapFontData&>(rawBase);
		auto font = std::make_unique<BitmapFont>();
		bool ok = false;

		if (raw.isSingleFile) {
			ok = font->loadFromBMFontMemory(raw.bmfBytes.data(), raw.bmfBytes.size());
		} else {
			ok = font->loadFromMemory(
				raw.textureBytes.data(), raw.textureBytes.size(),
				raw.metricsBytes.data(), raw.metricsBytes.size()
			);
		}

		if (!ok) {
			BT_ERROR("AsyncBitmapFontLoader: loadFromMemory failed for '{}'", raw.assetID);
			return;
		}

		manager.add<BitmapFont>(raw.assetID, std::move(font));
		BT_DEBUG("AsyncBitmapFontLoader: '{}' ready", raw.assetID);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".bmf", ".fnt" };
	}

private:

#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackBitmapParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncBitmapFontLoader: BT_PACK_MODE requires PackBitmapParams.");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncBitmapFontLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto raw = std::make_unique<RawBitmapFontData>();

		if (pp->isSingleFile()) {
			auto packed = m_resolver->resolve(pp->bmfID);
			if (!packed) {
				BT_ERROR("AsyncBitmapFontLoader: '{}' not found in any mounted pack",
					pp->bmfID);
				return nullptr;
			}

			raw->bmfBytes = std::move(packed->bytes);
			raw->isSingleFile = true;
		} else {
			auto texPacked = m_resolver->resolve(pp->textureID);
			if (!texPacked) {
				BT_ERROR("AsyncBitmapFontLoader: texture '{}' not found in any mounted pack",
					pp->textureID);

				return nullptr;
			}

			auto metPacked = m_resolver->resolve(pp->metricsID);
			if (!metPacked) {
				BT_ERROR("AsyncBitmapFontLoader: metrics '{}' not found in any mounted pack",
					pp->metricsID);
				return nullptr;
			}

			raw->textureBytes = std::move(texPacked->bytes);
			raw->metricsBytes = std::move(metPacked->bytes);
			raw->isSingleFile = false;
		}

		raw->valid = true;
		return raw;
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		auto raw = std::make_unique<RawBitmapFontData>();

		if (const auto* bp = dynamic_cast<const BitmapParams*>(&params)) {
			if (!readFile(bp->texturePath.string(), raw->textureBytes) ||
			 !readFile(bp->metricsPath.string(), raw->metricsBytes))
				return nullptr;

			raw->isSingleFile = false;
			raw->valid = true;
			return raw;
		}

		if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			if (!readFile(pp->path.string(), raw->bmfBytes))
				return nullptr;

			raw->isSingleFile = true;
			raw->valid = true;
			return raw;
		}

		BT_ERROR("AsyncBitmapFontLoader: unrecognized LoadParams type");
		return nullptr;
	}

	static bool readFile(const std::string& path, std::vector<U8>& out) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) {
			BT_ERROR("AsyncBitmapFontLoader: cannot open '{}'", path);
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
			BT_ERROR("AsyncBitmapFontLoader: short read from '{}'", path);

		return ok;
	}
};

} // namespace Blackthorn::Fonts
