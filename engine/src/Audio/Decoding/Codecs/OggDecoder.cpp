#include "Audio/Decoding/Codecs/OggDecoder.h"

#include <vorbis/vorbisfile.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Decoding {

bool OggDecoder::decode(
	const std::filesystem::path& path,
	AudioData& data
) {
	const auto pathStr = path.string();

	OggVorbis_File vf;
	if (ov_fopen(pathStr.c_str(), &vf) != 0) {
		BT_ERROR("OggDecoder: ov_fopen failed for '{}'", pathStr);
		return false;
	}

	const vorbis_info* info = ov_info(&vf, -1);
	if (!info) {
		ov_clear(&vf);
		BT_ERROR("OggDecoder: ov_info returned null for '{}'", pathStr);
		return false;
	}

	const size_t sampleCount =
		static_cast<size_t>(ov_pcm_total(&vf, -1) * info->channels);

	data.samples.resize(sampleCount);

	size_t samplesRead = 0;
	int bitstream = 0;

	while (samplesRead < sampleCount) {
		const size_t remaining = sampleCount - samplesRead;

		long ret = ov_read(
			&vf,
			reinterpret_cast<char*>(data.samples.data() + samplesRead),
			static_cast<int>(remaining * sizeof(I16)),
			0, // little-endian,
			sizeof(I16), // 16-bit
			1, // signed
			&bitstream
		);

		if (ret == 0)
			break;

		if (ret < 0) {
			BT_WARN("OggDecoder: ov_read error {} for '{}'", ret, pathStr);
			break;
		}

		samplesRead += static_cast<size_t>(ret) / sizeof(I16);
	}

	ov_clear(&vf);

	return samplesRead > 0;
}

bool OggDecoder::getInfo(
	const std::filesystem::path &path,
	AudioMetadata &data
) {
	OggVorbis_File vf;
	if (ov_fopen(path.string().c_str(), &vf) != 0)
		return false;

	const vorbis_info* info = ov_info(&vf, -1);
	if (!info) {
		ov_clear(&vf);
		return false;
	}

	data.channels = static_cast<U32>(info->channels);
	data.sampleRate = static_cast<U32>(info->rate);
	data.frameCount = static_cast<U32>(ov_pcm_total(&vf, -1));

	ov_clear(&vf);
	return true;
}

} // namespace Blackthorn::Audio