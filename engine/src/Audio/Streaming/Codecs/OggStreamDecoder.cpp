#include "Audio/Streaming/Codecs/OggStreamDecoder.h"

#include <vorbis/vorbisfile.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

struct OggStreamDecoder::Impl {
	OggVorbis_File vf{};

	bool open = false;

	AudioMetadata metadata{};
};

OggStreamDecoder::OggStreamDecoder()
	: m_impl(std::make_unique<Impl>()) {}

OggStreamDecoder::~OggStreamDecoder() {
	close();
}

bool OggStreamDecoder::open(
	const std::filesystem::path& path
) {
	close();

	const auto pathStr = path.string();

	if (ov_fopen(pathStr.c_str(), &m_impl->vf) != 0) {
		BT_ERROR(
			"OggStreamDecoder: failed to open '{}'",
			pathStr
		);

		return false;
	}

	const vorbis_info* info =
		ov_info(&m_impl->vf, -1);

	if (!info) {
		ov_clear(&m_impl->vf);
		return false;
	}

	m_impl->metadata.channels =
		static_cast<U32>(info->channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(info->rate);

	const ogg_int64_t total =
		ov_pcm_total(&m_impl->vf, -1);

	m_impl->metadata.frameCount =
		(total > 0)
			? static_cast<U64>(total)
			: 0;

	m_impl->open = true;

	return true;
}

size_t OggStreamDecoder::readFrames(
	I16* dest,
	size_t frameCount
) {
	if (!m_impl->open)
		return 0;

	size_t totalFramesRead = 0;

	while (totalFramesRead < frameCount) {
		int bitstream = 0;

		const size_t remainingFrames =
			frameCount - totalFramesRead;

		const long bytesRead =
			ov_read(
				&m_impl->vf,
				reinterpret_cast<char*>(
					dest +
					(totalFramesRead *
					 m_impl->metadata.channels)
				),
				static_cast<int>(
					remainingFrames *
					m_impl->metadata.channels *
					sizeof(I16)
				),
				0,
				2,
				1,
				&bitstream
			);

		if (bytesRead <= 0)
			break;

		const size_t framesRead =
			static_cast<size_t>(bytesRead) /
			(sizeof(I16) *
			 m_impl->metadata.channels);

		totalFramesRead += framesRead;
	}

	return totalFramesRead;
}

bool OggStreamDecoder::seek(U64 frameOffset) {
	if (!m_impl->open)
		return false;

	const int result =
		ov_pcm_seek(
			&m_impl->vf,
			static_cast<ogg_int64_t>(frameOffset)
		);

	if (result != 0) {
		BT_WARN(
			"OggStreamDecoder: seek failed ({})",
			frameOffset
		);

		return false;
	}

	return true;
}

void OggStreamDecoder::close() {
	if (m_impl->open) {
		ov_clear(&m_impl->vf);
		m_impl->open = false;
	}

	m_impl->metadata = {};
}

AudioMetadata OggStreamDecoder::info() const {
	return m_impl->metadata;
}

bool OggStreamDecoder::isOpen() const {
	return m_impl->open;
}

} // namespace Blackthorn::Audio::Streaming