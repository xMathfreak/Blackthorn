#pragma once

#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <SDL3_image/SDL_image.h>

#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Texture.h"

namespace Blackthorn::Graphics {

struct BLACKTHORN_API RawTextureData : Assets::IRawAssetData {
	std::vector<U8> pixels;
	int width = 0;
	int height = 0;
	int channels = 0;
	TextureParams params;
	std::string srcPath;

	RawTextureData() = default;
};

/**
 * @brief Load parameters for a texture with explicit sampling settings.
 *
 * A plain AssetLoadParams is enough when the default TextureParams are fine.
 */
struct BLACKTHORN_API TextureLoadParams final : Assets::AssetLoadParams {
	TextureParams textureParams;

	TextureLoadParams(std::filesystem::path p, TextureParams tp = {})
		: Assets::AssetLoadParams(std::move(p))
		, textureParams(std::move(tp))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<TextureLoadParams>(*this);
	}
};

namespace Detail {

/**
 * @brief Extracts the canonical ID and texture settings from either params type.
 */
inline std::optional<std::pair<std::string, TextureParams>> textureRequest(const Assets::LoadParams& params) {
	const Assets::AssetLoadParams* base = nullptr;
	TextureParams settings;

	if (const auto* tp = dynamic_cast<const TextureLoadParams*>(&params)) {
		base = tp;
		settings = tp->textureParams;
	} else if (const auto* pp = dynamic_cast<const Assets::AssetLoadParams*>(&params)) {
		base = pp;
	} else {
		BT_ERROR("TextureLoader: unrecognized LoadParams type");
		return std::nullopt;
	}

	const auto id = Blackthorn::normalizeAssetPath(base->source.generic_string());
	if (!id) {
		BT_ERROR("TextureLoader: invalid asset path '{}'", base->source.generic_string());
		return std::nullopt;
	}

	return std::make_pair(*id, settings);
}

/**
 * @brief Decodes encoded image bytes (PNG, JPEG, ...) into RGBA8 pixels.
 *
 * @return The decoded texture data, or nullptr on failure (logged).
 */
inline std::unique_ptr<RawTextureData> decodeImage(
	const std::vector<U8>& bytes,
	const std::string& id,
	const TextureParams& settings
) {
	SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
	if (!io) {
		BT_ERROR("TextureLoader: SDL_IOFromConstMem failed for '{}': {}", id, SDL_GetError());
		return nullptr;
	}

	SDL_Surface* surface = IMG_Load_IO(io, true);
	if (!surface) {
		BT_ERROR("TextureLoader: IMG_Load_IO failed for '{}': {}", id, SDL_GetError());
		return nullptr;
	}

	SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(surface);

	if (!converted) {
		BT_ERROR("TextureLoader: SDL_ConvertSurface failed for '{}': {}", id, SDL_GetError());
		return nullptr;
	}

	auto raw = std::make_unique<RawTextureData>();
	raw->srcPath = id;
	raw->params = settings;
	raw->width = converted->w;
	raw->height = converted->h;
	raw->channels = 4;

	const size_t rowBytes = static_cast<size_t>(converted->w) * 4;
	raw->pixels.resize(rowBytes * static_cast<size_t>(converted->h));

	const U8* src = static_cast<const U8*>(converted->pixels);
	for (int row = 0; row < converted->h; ++row) {
		std::memcpy(
			raw->pixels.data() + static_cast<size_t>(row) * rowBytes,
			src + static_cast<size_t>(row) * converted->pitch,
			rowBytes
		);
	}

	SDL_DestroySurface(converted);
	raw->valid = true;
	return raw;
}

} // namespace Detail

/**
 * @brief Synchronous texture loader. Reads through the injected resolver.
 */
class BLACKTHORN_API TextureLoader final : public Assets::IAssetLoader<Texture> {
public:
	std::unique_ptr<Texture> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("TextureLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::textureRequest(params);
		if (!request)
			return nullptr;

		const auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		auto raw = Detail::decodeImage(bytes->bytes, request->first, request->second);
		if (!raw)
			return nullptr;

		auto texture = std::make_unique<Texture>();
		if (!texture->loadFromMemory(raw->width, raw->height, raw->channels, raw->pixels.data(), raw->params)) {
			BT_ERROR("TextureLoader: GPU upload failed for '{}'", request->first);
			return nullptr;
		}

		return texture;
	}
};

/**
 * @brief Asynchronous texture loader. Decodes on a worker, uploads on the main thread.
 */
class BLACKTHORN_API AsyncTextureLoader final : public Assets::IAsyncAssetLoader<Texture> {
public:
	AsyncTextureLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncTextureLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::textureRequest(params);
		if (!request)
			return nullptr;

		const auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		return Detail::decodeImage(bytes->bytes, request->first, request->second);
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawTextureData&>(rawBase);

		auto texture = std::make_unique<Texture>();
		if (!texture->loadFromMemory(raw.width, raw.height, raw.channels, raw.pixels.data(), raw.params)) {
			BT_ERROR("AsyncTextureLoader: GPU upload failed for '{}' (src: '{}')",
				raw.assetID, raw.srcPath);
			return;
		}

		manager.add<Texture>(raw.assetID, std::move(texture));

		BT_DEBUG("AsyncTextureLoader: '{}' ready: {}x{} RGBA (src: '{}')",
			raw.assetID, raw.width, raw.height, raw.srcPath);
	}
};

} // namespace Blackthorn::Graphics
