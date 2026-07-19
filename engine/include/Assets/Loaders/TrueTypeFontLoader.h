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
#include "Fonts/TrueTypeFont.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
	#include "Assets/PackMount.h"
#endif

namespace Blackthorn::Fonts {

struct BLACKTHORN_API TTFParams : Assets::LoadParams {
	std::filesystem::path path;
	int size;

	TTFParams(const std::filesystem::path& ttfPath, int pointSize)
		: path(ttfPath)
		, size(pointSize)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<TTFParams>(*this);
	}
};

struct BLACKTHORN_API PackTTFParams final : Assets::LoadParams {
	std::string packID;
	int pointSize;

	PackTTFParams(std::string id, int size)
		: packID(std::move(id))
		, pointSize(size)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackTTFParams>(*this);
	}
};

struct BLACKTHORN_API RawTTFData : Assets::IRawAssetData {
	std::vector<U8> fontBytes;
	int pointSize = 0;

	RawTTFData() = default;
};

class BLACKTHORN_API TrueTypeFontLoader final : public Assets::IAssetLoader<TrueTypeFont> {
public:
	std::unique_ptr<TrueTypeFont> load(const Assets::LoadParams& params) override {
		const auto& p = static_cast<const TTFParams&>(params);
		auto font = std::make_unique<TrueTypeFont>();
		font->loadFromFile(p.path, p.size);
		return font;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".ttf", ".otf" };
	}
};

class BLACKTHORN_API AsyncTrueTypeFontLoader final : public Assets::IAsyncAssetLoader<TrueTypeFont> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncTrueTypeFontLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncTrueTypeFontLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawTTFData&>(rawBase);
		auto font = std::make_unique<TrueTypeFont>();

		if (!font->loadFromMemory(raw.fontBytes.data(), raw.fontBytes.size(), raw.pointSize)) {
			BT_ERROR("AsyncTrueTypeFontLoader: loadFromMemory failed for '{}'", raw.assetID);
			return;
		}

		manager.add<TrueTypeFont>(raw.assetID, std::move(font));
		BT_DEBUG("AsyncTrueTypeFontLoader: '{}' ready at {}pt", raw.assetID, raw.pointSize);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".ttf", ".otf" };
	}

private:

#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackTTFParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncTrueTypeFontLoader: BT_PACK_MODE requires PackTTFParams.");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncTrueTypeFontLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto packed = m_resolver->resolve(pp->packID);
		if (!packed) {
			BT_ERROR("AsyncTrueTypeFontLoader: '{}' not found in any mounted pack", pp->packID);
			return nullptr;
		}

		auto raw = std::make_unique<RawTTFData>();
		raw->fontBytes = std::move(packed->bytes);
		raw->pointSize = pp->pointSize;
		raw->valid = true;
		return raw;
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		const TTFParams* tp = dynamic_cast<const TTFParams*>(&params);
		if (!tp) {
			BT_ERROR("AsyncTrueTypeFontLoader: expected TTFParams in debug mode");
			return nullptr;
		}

		std::FILE* f = std::fopen(tp->path.string().c_str(), "rb");
		if (!f) {
			BT_ERROR("AsyncTrueTypeFontLoader: cannot open '{}'", tp->path.string());
			return nullptr;
		}

		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);

		auto raw = std::make_unique<RawTTFData>();
		raw->fontBytes.resize(static_cast<size_t>(size));
		raw->pointSize = tp->size;

		if (std::fread(raw->fontBytes.data(), 1, raw->fontBytes.size(), f) != raw->fontBytes.size()) {
			BT_ERROR("AsyncTrueTypeFontLoader: short read from '{}'", tp->path.string());
			std::fclose(f);
			return nullptr;
		}

		std::fclose(f);
		raw->valid = true;
		return raw;
	}
};

} // namespace Blackthorn::Fonts