#include "Audio/Streaming/Codecs/Mp3StreamDecoder.h"

#include <dr_mp3.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

struct Mp3StreamDecoder::Impl {
	drmp3 mp3{};

	bool open = false;

	AudioMetadata metadata{};
};

Mp3StreamDecoder::Mp3StreamDecoder()
	: m_impl(std::make_unique<Impl>()) {}

Mp3StreamDecoder::~Mp3StreamDecoder() {
	close();
}

bool Mp3StreamDecoder::open(const std::filesystem::path& path) {
	close();

	const auto pathStr = path.string();

	if (!drmp3_init_file(&m_impl->mp3, pathStr.c_str(), nullptr)) {
		BT_ERROR(
			"Mp3StreamDecoder: failed to open '{}'",
			pathStr
		);

		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels =
		static_cast<U32>(m_impl->mp3.channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(m_impl->mp3.sampleRate);

	m_impl->metadata.frameCount =
		static_cast<U64>(m_impl->mp3.totalPCMFrameCount);

	BT_DEBUG(
		"Mp3StreamDecoder: opened '{}' [{} ch, {} Hz, {} frames]",
		pathStr,
		m_impl->metadata.channels,
		m_impl->metadata.sampleRate,
		m_impl->metadata.frameCount
	);

	return true;
}

size_t Mp3StreamDecoder::readFrames(
	I16* dest,
	size_t frameCount
) {
	if (!m_impl->open)
		return 0;

	const drmp3_uint64 read =
		drmp3_read_pcm_frames_s16(
			&m_impl->mp3,
			static_cast<drmp3_uint64>(frameCount),
			dest
		);

	return static_cast<size_t>(read);
}

bool Mp3StreamDecoder::seek(U64 frameOffset) {
	if (!m_impl->open)
		return false;

	const drmp3_bool32 ok =
		drmp3_seek_to_pcm_frame(
			&m_impl->mp3,
			static_cast<drmp3_uint64>(frameOffset)
		);

	if (ok != DRMP3_TRUE) {
		BT_WARN(
			"Mp3StreamDecoder: seek to frame {} failed",
			frameOffset
		);

		return false;
	}

	return true;
}

void Mp3StreamDecoder::close() {
	if (m_impl->open) {
		drmp3_uninit(&m_impl->mp3);
		m_impl->open = false;
	}

	m_impl->metadata = {};
}

AudioMetadata Mp3StreamDecoder::info() const {
	return m_impl->metadata;
}

bool Mp3StreamDecoder::isOpen() const {
	return m_impl->open;
}

} // Blackthorn::Audio