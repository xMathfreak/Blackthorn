#pragma once

#include <cstring>
#include <vector>

#include <SDL3_image/SDL_image.h>

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Texture.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
	#include "Assets/PackMount.h"
#endif

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

struct BLACKTHORN_API TextureLoadParams final : Assets::LoadParams {
	std::filesystem::path path;
	TextureParams textureParams;

	TextureLoadParams(std::filesystem::path p, TextureParams tp = {})
		: path(std::move(p))
		, textureParams(std::move(tp))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<TextureLoadParams>(*this);
	}
};

class BLACKTHORN_API TextureLoader final : public Assets::IAssetLoader<Texture> {
public:
	std::unique_ptr<Texture> load(const Assets::LoadParams& params) override {
		std::filesystem::path filePath;
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
#ifdef BT_PACK_MODE
	explicit AsyncTextureLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncTextureLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {

#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
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

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".png", ".bmp", ".jpg", ".jpeg", ".tga" };
	}

private:

#ifdef BT_PACK_MODE

	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const Assets::PackLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncTextureLoader: BT_PACK_MODE requires PackLoadParams, "
			 "got a different LoadParams type. Use manager.loadAsync<Texture>("
			 "\"runtime_id\", PackLoadParams(\"pack_id\")) instead of PathLoadParams.");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncTextureLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		std::optional<Assets::PackedAssetData> packed;
		if (!pp->packPath.empty()) {
			Assets::PackMount singleMount;
			if (singleMount.mount(pp->packPath, 0))
				packed = singleMount.read(
					[&]() -> uint64_t {
						return 0;
					}()
				);
			packed = m_resolver->resolve(pp->assetID);
		} else {
			packed = m_resolver->resolve(pp->assetID);
		}

		if (!packed) {
			BT_ERROR("AsyncTextureLoader: '{}' not found in any mounted pack", pp->assetID);
			return nullptr;
		}

		return decodeImageFromMemory(pp->assetID, packed->bytes, packed->sourcePath);
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		std::filesystem::path filePath;
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
		raw->srcPath = filePath.string();

		SDL_Surface* surface = IMG_Load(filePath.string().c_str());
		if (!surface) {
			BT_ERROR("AsyncTextureLoader: IMG_Load failed for '{}': {}",
				filePath.string(), SDL_GetError());
			return nullptr;
		}

		return convertAndCopySurface(std::move(raw), surface, filePath.string());
	}

	/**
	 * @brief Decodes an in-memory encoded image (PNG/JPEG/etc.) via SDL_image.
	 *
	 * The bytes from the pack are still an encoded container — zstd decompressed
	 * them back to their original PNG/JPG bytes. SDL_image reads those bytes
	 * through an SDL_IOStream without any temporary disk file.
	 */
	std::unique_ptr<Assets::IRawAssetData> decodeImageFromMemory(
		const std::string& assetID,
		const std::vector<U8>& bytes,
		const std::string& sourcePath
	) {
		auto raw = std::make_unique<RawTextureData>();
		raw->srcPath = sourcePath;

		SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
		if (!io) {
			BT_ERROR("AsyncTextureLoader: SDL_IOFromConstMem failed for '{}': {}",
				assetID, SDL_GetError());
			return nullptr;
		}

		SDL_Surface* surface = IMG_Load_IO(io, true);
		if (!surface) {
			BT_ERROR("AsyncTextureLoader: IMG_Load_IO failed for '{}': {}",
				assetID, SDL_GetError());

			return nullptr;
		}

		return convertAndCopySurface(std::move(raw), surface, assetID);
	}

	std::unique_ptr<Assets::IRawAssetData> convertAndCopySurface(
		std::unique_ptr<RawTextureData> raw,
		SDL_Surface* surface,
		const std::string& logID
	) {
		SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(surface);

		if (!converted) {
			BT_ERROR("AsyncTextureLoader: SDL_ConvertSurface failed for '{}': {}",
				logID, SDL_GetError());

			return nullptr;
		}

		raw->width = converted->w;
		raw->height = converted->h;
		raw->channels = 4;

		const int rowBytes = converted->w * 4;
		raw->pixels.resize(static_cast<size_t>(converted->w) * converted->h * 4);

		const U8* src = static_cast<const U8*>(converted->pixels);
		for (int row = 0; row < converted->h; ++row)
			std::memcpy(raw->pixels.data() + row * rowBytes, src + row * converted->pitch, rowBytes);

		SDL_DestroySurface(converted);
		raw->valid = true;
		return raw;
	}
};

} // namespace Blackthorn::Graphics
