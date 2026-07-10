#pragma once

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Audio/Resources/AudioClip.h"
#include "Audio/Resources/AudioData.h"

namespace Blackthorn::Audio {

struct BLACKTHORN_API AudioParams : Assets::LoadParams {
	std::filesystem::path path;
	bool isPCM = false;

	AudioParams(const std::filesystem::path& filePath, bool loadPCM = false)
		: path(filePath)
		, isPCM(loadPCM)
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<AudioParams>(*this);
	}
};

struct BLACKTHORN_API RawAudioData : Assets::IRawAssetData {
	std::filesystem::path path;
	std::optional<AudioData> data;
	AudioMetadata metadata;
};

class BLACKTHORN_API AudioLoader final : public Assets::IAssetLoader<AudioClip> {
public:
	std::unique_ptr<AudioClip> load(const Assets::LoadParams& params) override {
		std::unique_ptr<AudioClip> clip = std::make_unique<AudioClip>();
		if (const AudioParams* ap = dynamic_cast<const AudioParams*>(&params)) {
			if (!clip->load(ap->path))
				return nullptr;

			if (ap->isPCM)
				clip->loadPCM();

			return clip;
		} else if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			if (!clip->load(pp->path))
				return nullptr;

			return clip;
		}

		clip.reset();
		return nullptr;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".wav", ".mp3", ".ogg", ".flac"};
	}
};

class BLACKTHORN_API AsyncAudioLoader final : public Assets::IAsyncAssetLoader<AudioClip> {
public:
	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		std::filesystem::path filePath;
		bool loadPCM = false;

		if (const auto* ap = dynamic_cast<const AudioParams*>(&params)) {
			filePath = ap->path;
			loadPCM = ap->isPCM;
		} else if (const auto* pp = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			filePath = pp->path;
		}

		auto raw = std::make_unique<RawAudioData>();
		raw->path = filePath;

		if (!Decoding::AudioDecoder::getInfo(raw->path, raw->metadata)) {
			return nullptr;
		}

		if (loadPCM) {
			AudioData data;
			if (!Decoding::AudioDecoder::decode(raw->path, data)) {
				return nullptr;
			}

			raw->data = std::move(data);
		}

		raw->valid = true;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto raw = static_cast<RawAudioData&>(rawBase);
		auto clip = std::make_unique<AudioClip>();
		clip->loadFromMemory(raw.path, raw.metadata, raw.data);

		manager.add<AudioClip>(raw.assetID, std::move(clip));
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".wav", ".mp3", ".ogg", ".flac"};
	}
};

} // namespace Blackthorn::Audio