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
			"WavDecoder: WAV read only {}/{} frames from '{}'",
			framesRead, wav.totalPCMFrameCount, pathStr
		);
	}

	return true;
}

bool WavDecoder::getInfo(
	const std::filesystem::path &path,
	AudioMetadata &data
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

} // namespace Blackthorn::Audio