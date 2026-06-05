#pragma once

#include "Assets/AssetManager.h"
#include "Audio/Resources/AudioClip.h"
#include "Assets/IAssetLoader.h"

namespace Blackthorn::Audio {

struct BLACKTHORN_API RawAudio : Assets::IRawAssetData {
	std::string path;

	RawAudio(const std::string& p)
		: path(p)
	{}
};

class BLACKTHORN_API AudioLoader final : public Assets::IAssetLoader<AudioClip> {
public:
	std::unique_ptr<AudioClip> load(const Assets::LoadParams& params) override {
		const auto path = static_cast<const Assets::PathLoadParams*>(&params)->path;
		std::unique_ptr<AudioClip> clip = std::make_unique<AudioClip>(path);
		return clip;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".wav", ".mp3", ".ogg", ".flac"};
	}
};

class BLACKTHORN_API AsyncAudioLoader final : public Assets::IAsyncAssetLoader<AudioClip> {
public:
	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) {
		const auto path = static_cast<const Assets::PathLoadParams*>(&params)->path;

		std::unique_ptr<RawAudio> raw = std::make_unique<RawAudio>(path);
		raw->valid = true;

		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) {
		auto& raw = static_cast<RawAudio&>(rawBase);
		manager.add<AudioClip>(rawBase.assetID, std::make_unique<AudioClip>(raw.path));
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".wav", ".mp3", ".ogg", ".flac"};
	}
};

} // namespace Blackthorn::Audio