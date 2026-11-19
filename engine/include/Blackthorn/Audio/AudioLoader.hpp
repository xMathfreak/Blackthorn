#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Blackthorn/Assets/AssetManager.hpp"
#include "Blackthorn/Assets/AssetResolver.hpp"
#include "Blackthorn/Assets/IAssetLoader.hpp"
#include "Blackthorn/Assets/LoadParams.hpp"
#include "Blackthorn/Assets/RawAssetData.hpp"
#include "AssetNormalize.hpp"
#include "Blackthorn/Audio/Decoding/AudioDecoder.hpp"
#include "Blackthorn/Audio/Resources/AudioClip.hpp"
#include "Blackthorn/Audio/Resources/AudioData.hpp"
#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Debug/Logger.hpp"

namespace Blackthorn::Audio {

/**
 * @brief Load parameters for an audio clip.
 *
 * isPCM = true decodes the whole clip to PCM up front. Otherwise the clip
 * streams from its compressed bytes.
 */
struct BLACKTHORN_API AudioParams final : Assets::AssetLoadParams {
	bool isPCM = false;

	AudioParams(std::filesystem::path filePath, bool loadPCM = false)
		: Assets::AssetLoadParams(std::move(filePath))
		, isPCM(loadPCM)
	{}

	[[nodiscard]] std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<AudioParams>(*this);
	}
};

struct BLACKTHORN_API RawAudioData : Assets::IRawAssetData {
	std::string srcPath;
	std::optional<AudioData> data;
	AudioMetadata metadata;
	std::optional<std::vector<U8>> compressedBytes;
};

namespace Detail {

/**
 * @brief Canonical ID and PCM flag for an audio request.
 */
inline std::optional<std::pair<std::string, bool>> audioRequest(const Assets::LoadParams& params) {
	const auto* base = dynamic_cast<const Assets::AssetLoadParams*>(&params);
	if (!base) {
		BT_ERROR("AudioLoader: unrecognized LoadParams type");
		return std::nullopt;
	}

	const auto id = Blackthorn::normalizeAssetPath(base->source.generic_string());
	if (!id) {
		BT_ERROR("AudioLoader: invalid asset path '{}'", base->source.generic_string());
		return std::nullopt;
	}

	const auto* audio = dynamic_cast<const AudioParams*>(&params);
	return std::make_pair(*id, audio && audio->isPCM);
}

/**
 * @brief Decodes resolver bytes into a RawAudioData.
 *
 * Both loaders use this, so the compressed bytes are always kept for streaming.
 */
inline std::unique_ptr<RawAudioData> decodeAudio(
	const std::string& assetID,
	std::vector<U8> bytes,
	bool loadPCM
) {
	auto raw = std::make_unique<RawAudioData>();
	raw->srcPath = assetID;

	if (!Decoding::AudioDecoder::getInfoFromMemory(bytes.data(), bytes.size(), raw->metadata)) {
		BT_ERROR("AudioLoader: getInfoFromMemory failed for '{}'", assetID);
		return nullptr;
	}

	if (loadPCM) {
		AudioData data;
		if (!Decoding::AudioDecoder::decodeFromMemory(bytes.data(), bytes.size(), data)) {
			BT_ERROR("AudioLoader: decodeFromMemory failed for '{}'", assetID);
			return nullptr;
		}

		raw->data = std::move(data);
	}

	raw->compressedBytes = std::move(bytes);
	raw->valid = true;
	return raw;
}

} // namespace Detail

/**
 * @brief Synchronous audio loader. Reads through the resolver.
 */
class BLACKTHORN_API AudioLoader final : public Assets::IAssetLoader<AudioClip> {
public:
	std::unique_ptr<AudioClip> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AudioLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::audioRequest(params);
		if (!request)
			return nullptr;

		auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		auto raw = Detail::decodeAudio(request->first, std::move(bytes->bytes), request->second);
		if (!raw)
			return nullptr;

		auto clip = std::make_unique<AudioClip>();
		clip->loadFromMemory(
			raw->srcPath,
			raw->metadata,
			std::move(raw->data),
			std::move(raw->compressedBytes)
		);

		return clip;
	}
};

/**
 * @brief Asynchronous audio loader. Reads and decodes on a worker, creates the clip on the main thread.
 */
class BLACKTHORN_API AsyncAudioLoader final : public Assets::IAsyncAssetLoader<AudioClip> {
public:
	AsyncAudioLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncAudioLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::audioRequest(params);
		if (!request)
			return nullptr;

		auto bytes = resolver->resolve(request->first);
		if (!bytes)
			return nullptr;

		return Detail::decodeAudio(request->first, std::move(bytes->bytes), request->second);
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawAudioData&>(rawBase);

		auto clip = std::make_unique<AudioClip>();
		clip->loadFromMemory(
			raw.srcPath,
			raw.metadata,
			std::move(raw.data),
			std::move(raw.compressedBytes)
		);

		manager.add<AudioClip>({raw.assetName}, std::move(clip));
	}
};

} // namespace Blackthorn::Audio
