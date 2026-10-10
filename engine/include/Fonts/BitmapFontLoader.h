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
#include "Fonts/BitmapFont.h"

namespace Blackthorn::Fonts {

/**
 * @brief Load parameters for a bitmap font split into a texture and a metrics file.
 *
 * The texture is the asset's source path. The metrics file is a second canonical ID.
 * For a single packed `.btf` file, use plain AssetLoadParams instead.
 */
struct BLACKTHORN_API BitmapParams final : Assets::AssetLoadParams {
	std::filesystem::path metricsPath;

	BitmapParams(std::filesystem::path texture, std::filesystem::path metrics)
		: Assets::AssetLoadParams(std::move(texture))
		, metricsPath(std::move(metrics))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<BitmapParams>(*this);
	}
};

struct BLACKTHORN_API RawBitmapFontData : Assets::IRawAssetData {
	std::vector<U8> btfBytes;      ///< Single-file `.btf` content.
	std::vector<U8> textureBytes;  ///< Texture content, for the split form.
	std::vector<U8> metricsBytes;  ///< Metrics content, for the split form.

	bool isSingleFile = false;

	RawBitmapFontData() = default;
};

namespace Detail {

/**
 * @brief Canonical IDs for a bitmap font request.
 *
 * singleFile is true for plain AssetLoadParams (one `.btf`). Otherwise the
 * request is a texture plus a metrics file.
 */
struct BitmapRequest {
	bool        singleFile = false;
	std::string primaryID;   ///< The `.btf` file, or the texture.
	std::string metricsID;   ///< Metrics file, if not singleFile.
};

inline std::optional<BitmapRequest> bitmapRequest(const Assets::LoadParams& params) {
	if (const auto* bp = dynamic_cast<const BitmapParams*>(&params)) {
		const auto texture = Blackthorn::normalizeAssetPath(bp->source.generic_string());
		const auto metrics = Blackthorn::normalizeAssetPath(bp->metricsPath.generic_string());
		if (!texture || !metrics) {
			BT_ERROR("BitmapFontLoader: invalid path (texture '{}', metrics '{}')",
				bp->source.generic_string(), bp->metricsPath.generic_string());
			return std::nullopt;
		}

		return BitmapRequest{ false, *texture, *metrics };
	}

	if (const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params)) {
		const auto id = Blackthorn::normalizeAssetPath(pp->source.generic_string());
		if (!id) {
			BT_ERROR("BitmapFontLoader: invalid asset path '{}'", pp->source.generic_string());
			return std::nullopt;
		}

		return BitmapRequest{ true, *id, {} };
	}

	BT_ERROR("BitmapFontLoader: unrecognized LoadParams type");
	return std::nullopt;
}

/**
 * @brief Fetches the bytes for a request into @p raw through the resolver.
 */
inline bool fetchBitmapBytes(Assets::AssetResolver& resolver, const BitmapRequest& req, RawBitmapFontData& raw) {
	raw.isSingleFile = req.singleFile;

	auto primary = resolver.resolve(req.primaryID);
	if (!primary)
		return false;

	if (req.singleFile) {
		raw.btfBytes = std::move(primary->bytes);
		return true;
	}

	auto metrics = resolver.resolve(req.metricsID);
	if (!metrics)
		return false;

	raw.textureBytes = std::move(primary->bytes);
	raw.metricsBytes = std::move(metrics->bytes);
	return true;
}

/**
 * @brief Builds a BitmapFont from fetched bytes. Used by both loaders.
 */
inline std::unique_ptr<BitmapFont> buildBitmapFont(const RawBitmapFontData& raw) {
	auto font = std::make_unique<BitmapFont>();

	const bool ok = raw.isSingleFile
		? font->loadFromBTFontMemory(raw.btfBytes.data(), raw.btfBytes.size())
		: font->loadFromMemory(
			raw.textureBytes.data(), raw.textureBytes.size(),
			raw.metricsBytes.data(), raw.metricsBytes.size()
		);

	return ok ? std::move(font) : nullptr;
}

} // namespace Detail

/**
 * @brief Synchronous bitmap font loader. Reads bytes through the resolver.
 */
class BLACKTHORN_API BitmapFontLoader final : public Assets::IAssetLoader<BitmapFont> {
public:
	std::unique_ptr<BitmapFont> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("BitmapFontLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto req = Detail::bitmapRequest(params);
		if (!req)
			return nullptr;

		RawBitmapFontData raw;
		if (!Detail::fetchBitmapBytes(*resolver, *req, raw))
			return nullptr;

		auto font = Detail::buildBitmapFont(raw);
		if (!font)
			BT_ERROR("BitmapFontLoader: failed to parse '{}'", req->primaryID);

		return font;
	}
};

/**
 * @brief Asynchronous bitmap font loader. Reads on a worker, builds on the main thread.
 */
class BLACKTHORN_API AsyncBitmapFontLoader final : public Assets::IAsyncAssetLoader<BitmapFont> {
public:
	AsyncBitmapFontLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncBitmapFontLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto req = Detail::bitmapRequest(params);
		if (!req)
			return nullptr;

		auto raw = std::make_unique<RawBitmapFontData>();
		if (!Detail::fetchBitmapBytes(*resolver, *req, *raw))
			return nullptr;

		raw->valid = true;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawBitmapFontData&>(rawBase);

		auto font = Detail::buildBitmapFont(raw);
		if (!font) {
			BT_ERROR("AsyncBitmapFontLoader: failed to parse '{}'", raw.assetID);
			return;
		}

		manager.add<BitmapFont>(raw.assetID, std::move(font));
		BT_DEBUG("AsyncBitmapFontLoader: '{}' ready", raw.assetID);
	}
};

} // namespace Blackthorn::Fonts
