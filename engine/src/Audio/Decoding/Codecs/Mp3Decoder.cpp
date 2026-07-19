#include "Audio/Decoding/Codecs/Mp3Decoder.h"

#include <dr_mp3.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Decoding {

bool Mp3Decoder::decode(
	const std::filesystem::path& path,
	AudioData& data
) {
	const auto pathStr = path.string();

	drmp3_config config = {};
	drmp3_uint64 totalFrames = 0;

	drmp3_int16* raw = drmp3_open_file_and_read_pcm_frames_s16(
		pathStr.c_str(), &config, &totalFrames, nullptr
	);

	if (!raw) {
		BT_ERROR("Mp3Decoder: drmp3_open_file_and_read_pcm_frames_s16 failed for '{}'", pathStr);
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(totalFrames) * config.channels;
	data.samples.assign(raw, raw + sampleCount);
	drmp3_free(raw, nullptr);

	return true;
}

bool Mp3Decoder::getInfo(
	const std::filesystem::path& path,
	AudioMetadata& data
) {
	drmp3 mp3;
	if (!drmp3_init_file(&mp3, path.string().c_str(), nullptr))
		return false;

	const drmp3_uint32 sampleRate = mp3.sampleRate;
	const drmp3_uint32 channels = mp3.channels;

	if (sampleRate == 0 || channels == 0) {
		drmp3_uninit(&mp3);
		return false;
	}

	data.sampleRate = sampleRate;
	data.channels = channels;
	data.frameCount = drmp3_get_pcm_frame_count(&mp3);

	drmp3_uninit(&mp3);
	return true;
}

bool Mp3Decoder::decodeFromMemory(
	const U8* src,
	size_t srcSize,
	AudioData& data
) {
	drmp3_config config = {};
	drmp3_uint64 totalFrames = 0;

	drmp3_int16* raw = drmp3_open_memory_and_read_pcm_frames_s16(
		src, srcSize,
		&config, &totalFrames,
		nullptr
	);

	if (!raw) {
		BT_ERROR("Mp3Decoder: drmp3_open_memory_and_read_pcm_frames_s16 failed");
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(totalFrames) * config.channels;
	data.samples.assign(raw, raw + sampleCount);
	drmp3_free(raw, nullptr);

	return true;
}

bool Mp3Decoder::getInfoFromMemory(
	const U8* src,
	size_t srcSize,
	AudioMetadata& data
) {
	drmp3 mp3;
	if (!drmp3_init_memory(&mp3, src, srcSize, nullptr)) {
		BT_ERROR("Mp3Decoder: drmp3_init_memory failed");
		return false;
	}

	if (mp3.sampleRate == 0 || mp3.channels == 0) {
		BT_ERROR("Mp3Decoder: drmp3_init_memory returned invalid stream properties");
		drmp3_uninit(&mp3);
		return false;
	}

	data.sampleRate = mp3.sampleRate;
	data.channels = mp3.channels;
	data.frameCount = drmp3_get_pcm_frame_count(&mp3);

	drmp3_uninit(&mp3);
	return true;
}

} // namespace Blackthorn::Audio::Decoding