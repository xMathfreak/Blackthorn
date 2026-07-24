#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Audio/Decoding/AudioDecoder.h"
#include "Audio/Resources/AudioClip.h"
#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
	#include "Assets/PackMount.h"
#endif

namespace Blackthorn::Audio {

struct BLACKTHORN_API AudioParams : Assets::LoadParams {
	std::filesystem::path path;
	bool isPCM = false;

	AudioParams(const std::filesystem::path& filePath, bool loadPCM = false)
		: path(filePath)
		, isPCM(loadPCM)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<AudioParams>(*this);
	}
};

struct BLACKTHORN_API RawAudioData : Assets::IRawAssetData {
	std::string srcPath;
	std::optional<AudioData> data;
	AudioMetadata metadata;
	std::optional<std::vector<U8>> compressedBytes;
};

class BLACKTHORN_API AudioLoader final : public Assets::IAssetLoader<AudioClip> {
public:
	std::unique_ptr<AudioClip> load(const Assets::LoadParams& params) override {
		auto clip = std::make_unique<AudioClip>();

		if (const auto* ap = dynamic_cast<const AudioParams*>(&params)) {
			if (!clip->load(ap->path))
				return nullptr;

			if (ap->isPCM)
				clip->loadPCM();

			return clip;
		}

		if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			if (!clip->load(pp->path))
				return nullptr;

			return clip;
		}

		return nullptr;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".wav", ".mp3", ".ogg", ".flac" };
	}
};

class BLACKTHORN_API AsyncAudioLoader final : public Assets::IAsyncAssetLoader<AudioClip> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncAudioLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncAudioLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawAudioData&>(rawBase);
		auto clip = std::make_unique<AudioClip>();
		clip->loadFromMemory(raw.srcPath, raw.metadata, raw.data, std::move(raw.compressedBytes));
		manager.add<AudioClip>(raw.assetID, std::move(clip));
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".wav", ".mp3", ".ogg", ".flac" };
	}

private:

#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const Assets::PackLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncAudioLoader: BT_PACK_MODE requires PackLoadParams. "
			 "Use PackLoadParams(\"pack_id\") instead of PathLoadParams.");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncAudioLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto packed = m_resolver->resolve(pp->assetID);
		if (!packed) {
			BT_ERROR("AsyncAudioLoader: '{}' not found in any mounted pack", pp->assetID);
			return nullptr;
		}

		return decodeAudioFromMemory(pp->assetID, std::move(packed->bytes), packed->sourcePath, false);
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		std::filesystem::path filePath;
		bool loadPCM = false;

		if (const auto* ap = dynamic_cast<const AudioParams*>(&params)) {
			filePath = ap->path;
			loadPCM = ap->isPCM;
		} else if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			filePath = pp->path;
		} else {
			BT_ERROR("AsyncAudioLoader: unrecognized LoadParams type");
			return nullptr;
		}

		auto raw = std::make_unique<RawAudioData>();
		raw->srcPath = filePath.string();

		if (!Decoding::AudioDecoder::getInfo(filePath, raw->metadata))
			return nullptr;

		if (loadPCM) {
			AudioData data;
			if (!Decoding::AudioDecoder::decode(filePath, data))
				return nullptr;

			raw->data = std::move(data);
		}

		raw->valid = true;
		return raw;
	}

	std::unique_ptr<Assets::IRawAssetData> decodeAudioFromMemory(
		const std::string& assetID,
		std::vector<U8> bytes,
		const std::string& sourcePath,
		bool loadPCM
	) {
		auto raw = std::make_unique<RawAudioData>();
		raw->srcPath = sourcePath;

		if (!Decoding::AudioDecoder::getInfoFromMemory(bytes.data(), bytes.size(), raw->metadata)) {
			BT_ERROR("AsyncAudioLoader: getInfoFromMemory failed for '{}'", assetID);
			return nullptr;
		}

		if (loadPCM) {
			AudioData data;
			if (!Decoding::AudioDecoder::decodeFromMemory(bytes.data(), bytes.size(), data)) {
				BT_ERROR("AsyncAudioLoader: decodeFromMemory failed for '{}'", assetID);
				return nullptr;
			}

			raw->data = std::move(data);
		}

		raw->compressedBytes = std::move(bytes);

		raw->valid = true;
		return raw;
	}
};

} // namespace Blackthorn::Audio
