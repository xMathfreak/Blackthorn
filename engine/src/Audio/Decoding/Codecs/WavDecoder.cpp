#include "Audio/Decoding/Codecs/WavDecoder.h"

#include <dr_wav.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Decoding {

bool WavDecoder::decode(
	const std::filesystem::path& path,
	AudioData& data
) {
	const auto pathStr = path.string();
	drwav wav;
	if (!drwav_init_file(&wav, pathStr.c_str(), nullptr)) {
		BT_ERROR("WavDecoder: drwav_init_file failed for '{}'", pathStr);
		return false;
	}

	const size_t sampleCount =
		static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels;

	data.samples.resize(sampleCount);

	const drwav_uint64 framesRead = drwav_read_pcm_frames_s16(
		&wav,
		wav.totalPCMFrameCount,
		data.samples.data()
	);

	drwav_uninit(&wav);

	if (framesRead != wav.totalPCMFrameCount) {
		BT_WARN(
			"WavDecoder: read only {}/{} frames from '{}'",
			framesRead, wav.totalPCMFrameCount, pathStr
		);
	}

	return true;
}

bool WavDecoder::getInfo(
	const std::filesystem::path& path,
	AudioMetadata& data
) {
	drwav wav;
	if (!drwav_init_file(&wav, path.string().c_str(), nullptr))
		return false;

	data.channels = wav.channels;
	data.sampleRate = wav.sampleRate;
	data.frameCount = wav.totalPCMFrameCount;

	drwav_uninit(&wav);
	return true;
}

bool WavDecoder::decodeFromMemory(
	const U8* src,
	size_t srcSize,
	AudioData& data
) {
	unsigned int channels = 0;
	unsigned int sampleRate = 0;
	drwav_uint64 totalFrames = 0;

	drwav_int16* raw = drwav_open_memory_and_read_pcm_frames_s16(
		src, srcSize,
		&channels, &sampleRate, &totalFrames,
		nullptr
	);

	if (!raw) {
		BT_ERROR("WavDecoder: drwav_open_memory_and_read_pcm_frames_s16 failed");
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(totalFrames) * channels;
	data.samples.assign(raw, raw + sampleCount);
	drwav_free(raw, nullptr);

	return true;
}

bool WavDecoder::getInfoFromMemory(
	const U8* src,
	size_t srcSize,
	AudioMetadata& data
) {
	drwav wav;
	if (!drwav_init_memory(&wav, src, srcSize, nullptr)) {
		BT_ERROR("WavDecoder: drwav_init_memory failed");
		return false;
	}

	data.channels = wav.channels;
	data.sampleRate = wav.sampleRate;
	data.frameCount = wav.totalPCMFrameCount;

	drwav_uninit(&wav);
	return true;
}

} // namespace Blackthorn::Audio::Decoding