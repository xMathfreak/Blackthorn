#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Fonts/TrueTypeFont.h"

namespace Blackthorn::Fonts {

/**
 * @brief Load parameters for a TrueType font at a fixed point size.
 *
 * The font file is the asset's source path, so the ID is the font's canonical path.
 */
struct BLACKTHORN_API TTFParams final : Assets::AssetLoadParams {
	int size;

	TTFParams(std::filesystem::path ttfPath, int pointSize)
		: Assets::AssetLoadParams(std::move(ttfPath))
		, size(pointSize)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<TTFParams>(*this);
	}
};

struct BLACKTHORN_API RawTTFData : Assets::IRawAssetData {
	std::vector<U8> fontBytes;
	int pointSize = 0;

	RawTTFData() = default;
};

namespace Detail {

/**
 * @brief Canonical ID and point size for a TrueType request.
 */
inline std::optional<std::pair<std::string, int>> ttfRequest(const Assets::LoadParams& params) {
	const auto* tp = dynamic_cast<const TTFParams*>(&params);
	if (!tp) {
		BT_ERROR("TrueTypeFontLoader: expected TTFParams");
		return std::nullopt;
	}

	const auto id = Blackthorn::normalizeAssetPath(tp->source.generic_string());
	if (!id) {
		BT_ERROR("TrueTypeFontLoader: invalid asset path '{}'", tp->source.generic_string());
		return std::nullopt;
	}

	return std::make_pair(*id, tp->size);
}

} // namespace Detail

/**
 * @brief Synchronous TrueType loader. Reads the font through the resolver.
 */
class BLACKTHORN_API TrueTypeFontLoader final : public Assets::IAssetLoader<TrueTypeFont> {
public:
	std::unique_ptr<TrueTypeFont> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("TrueTypeFontLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::ttfRequest(params);
		if (!request)
			return nullptr;

		const auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		auto font = std::make_unique<TrueTypeFont>();
		if (!font->loadFromMemory(bytes->bytes.data(), bytes->bytes.size(), request->second)) {
			BT_ERROR("TrueTypeFontLoader: failed to parse '{}'", request->first);
			return nullptr;
		}

		return font;
	}
};

/**
 * @brief Asynchronous TrueType loader. Reads on a worker, builds on the main thread.
 */
class BLACKTHORN_API AsyncTrueTypeFontLoader final : public Assets::IAsyncAssetLoader<TrueTypeFont> {
public:
	AsyncTrueTypeFontLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncTrueTypeFontLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::ttfRequest(params);
		if (!request)
			return nullptr;

		const auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		auto raw = std::make_unique<RawTTFData>();
		raw->fontBytes = std::move(bytes->bytes);
		raw->pointSize = request->second;
		raw->valid = true;
		return raw;
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
};

} // namespace Blackthorn::Fonts
