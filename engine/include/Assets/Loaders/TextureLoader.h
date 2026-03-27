#pragma once

#include <cstring>

#include <SDL3_image/SDL_image.h>

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Texture.h"

namespace Blackthorn::Graphics {

struct BLACKTHORN_API RawTextureData : Assets::IRawAssetData {
	std::vector<Uint8> pixels;
	int width = 0;
	int height = 0;
	int channels = 0;
	TextureParams params;

	std::string srcPath;

	RawTextureData() = default;
};

struct BLACKTHORN_API TextureLoadParams final : Assets::LoadParams {
	std::string		path;
	TextureParams	textureParams;

	TextureLoadParams(std::string p, TextureParams tp = {})
		: path(std::move(p))
		, textureParams(std::move(tp))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<TextureLoadParams>(*this);
	}
};

class BLACKTHORN_API TextureLoader final : public Assets::IAssetLoader<Texture> {
public:
	std::unique_ptr<Graphics::Texture> load(const Assets::LoadParams& params) override {
		std::string filePath;
		TextureParams texParams;

		if (const auto* tp = dynamic_cast<const TextureLoadParams*>(&params)) {
			filePath = tp->path;
			texParams = tp->textureParams;
		} else if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			filePath = pp->path;
		} else {
			BT_ERROR("TextureLoader: unrecognized LoadParams type");
			return nullptr;
		}

		return std::make_unique<Texture>(filePath, texParams);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".png", ".bmp", ".jpg", ".jpeg", ".tga" };
	}
};

class BLACKTHORN_API AsyncTextureLoader final : public Assets::IAsyncAssetLoader<Texture> {
public:
	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		std::string filePath;
		TextureParams texParams;

		if (const auto* tp = dynamic_cast<const TextureLoadParams*>(&params)) {
			filePath = tp->path;
			texParams = tp->textureParams;
		} else if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			filePath = pp->path;
		} else {
			BT_ERROR("AsyncTextureLoader: unrecognized LoadParams type");
			return nullptr;
		}

		auto raw = std::make_unique<RawTextureData>();
		raw->params = texParams;
		raw->srcPath = filePath;

		SDL_Surface* surface = IMG_Load(filePath.c_str());
		if (!surface) {
			BT_ERROR(
				"AsyncTextureLoader: IMG_Load fialed for '{}': {}",
				filePath, SDL_GetError()
			);

			return nullptr;
		}

		SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(surface);

		if (!converted) {
			BT_ERROR(
				"AsyncTextureLoader: SDL_ConvertSurface failed for '{}': {}",
				filePath, SDL_GetError()
			);

			return nullptr;
		}

		raw->width = converted->w;
		raw->height = converted->h;
		raw->channels = 4;

		const int rowBytes = converted->w * 4;
		const int pitch = converted->pitch;
		const Uint8* src = static_cast<const Uint8*>(converted->pixels);

		raw->pixels.resize(static_cast<size_t>(converted->w) * converted->h * 4);

		for (int row = 0; row < converted->h; ++row) {
			memcpy(
				raw->pixels.data() + row * rowBytes,
				src + row * pitch,
				rowBytes
			);
		}

		SDL_DestroySurface(converted);
		raw->valid = true;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawTextureData&>(rawBase);

		auto texture = std::make_unique<Texture>();

		if (!texture->loadFromMemory(raw.width, raw.height, raw.channels, raw.pixels.data(), raw.params)) {
			BT_ERROR(
				"AsyncTextureLoader: GPU upload failed for '{}' (source: '{}')",
				raw.assetID, raw.srcPath
			);

			return;
		}

		manager.add<Texture>(raw.assetID, std::move(texture));

		BT_DEBUG(
			"AsyncTextureLoader: '{}' ready — {}x{} RGBA, source '{}'",
			raw.assetID, raw.width, raw.height, raw.srcPath
		);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".png", ".bmp", ".jpg", ".jpeg", ".tga" };
	}
};

} // namespace Blackthorn::Graphics