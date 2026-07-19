#include "Audio/Decoding/Codecs/FlacDecoder.h"

#include <dr_flac.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Decoding {

bool FlacDecoder::decode(
	const std::filesystem::path& path,
	AudioData& data
) {
	const auto pathStr = path.string();

	unsigned int channels = 0;
	unsigned int sampleRate = 0;
	drflac_uint64 totalFrames = 0;

	drflac_int16* raw = drflac_open_file_and_read_pcm_frames_s16(
		pathStr.c_str(), &channels, &sampleRate, &totalFrames, nullptr
	);

	if (!raw) {
		BT_ERROR("FlacDecoder: drflac_open_file_and_read_pcm_frames_s16 failed for '{}'", pathStr);
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(totalFrames) * channels;
	data.samples.assign(raw, raw + sampleCount);
	drflac_free(raw, nullptr);

	return true;
}

bool FlacDecoder::getInfo(
	const std::filesystem::path& path,
	AudioMetadata& data
) {
	drflac* flac = drflac_open_file(path.string().c_str(), nullptr);
	if (!flac)
		return false;

	data.channels = flac->channels;
	data.sampleRate = flac->sampleRate;
	data.frameCount = flac->totalPCMFrameCount;

	drflac_close(flac);
	return true;
}

bool FlacDecoder::decodeFromMemory(
	const U8* src,
	size_t srcSize,
	AudioData& data
) {
	unsigned int channels = 0;
	unsigned int sampleRate = 0;
	drflac_uint64 totalFrames = 0;

	drflac_int16* raw = drflac_open_memory_and_read_pcm_frames_s16(
		src, srcSize,
		&channels, &sampleRate, &totalFrames,
		nullptr
	);

	if (!raw) {
		BT_ERROR("FlacDecoder: drflac_open_memory_and_read_pcm_frames_s16 failed");
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(totalFrames) * channels;
	data.samples.assign(raw, raw + sampleCount);
	drflac_free(raw, nullptr);

	return true;
}

bool FlacDecoder::getInfoFromMemory(
	const U8* src,
	size_t srcSize,
	AudioMetadata& data
) {
	drflac* flac = drflac_open_memory(src, srcSize, nullptr);
	if (!flac) {
		BT_ERROR("FlacDecoder: drflac_open_memory failed");
		return false;
	}

	data.channels = flac->channels;
	data.sampleRate = flac->sampleRate;
	data.frameCount = flac->totalPCMFrameCount;

	drflac_close(flac);
	return true;
}

} // namespace Blackthorn::Audio::Decoding
