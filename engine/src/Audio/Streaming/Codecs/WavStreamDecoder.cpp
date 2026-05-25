#include "Audio/Streaming/Codecs/WavStreamDecoder.h"

#include <dr_wav.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

struct WavStreamDecoder::Impl {
	drwav wav{};

	bool open = false;

	AudioMetadata metadata{};
};

WavStreamDecoder::WavStreamDecoder()
	: m_impl(std::make_unique<Impl>()) {}

WavStreamDecoder::~WavStreamDecoder() {
	close();
}

bool WavStreamDecoder::open(const std::filesystem::path& path) {
	close();

	const auto pathStr = path.string();

	if (!drwav_init_file(&m_impl->wav, pathStr.c_str(), nullptr)) {
		BT_ERROR(
			"WavStreamDecoder: failed to open '{}'",
			pathStr
		);

		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels =
		static_cast<U32>(m_impl->wav.channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(m_impl->wav.sampleRate);

	m_impl->metadata.frameCount =
		static_cast<U64>(m_impl->wav.totalPCMFrameCount);

	BT_DEBUG(
		"WavStreamDecoder: opened '{}' [{} ch, {} Hz, {} frames]",
		pathStr,
		m_impl->metadata.channels,
		m_impl->metadata.sampleRate,
		m_impl->metadata.frameCount
	);

	return true;
}

size_t WavStreamDecoder::readFrames(
	I16* dest,
	size_t frameCount
) {
	if (!m_impl->open)
		return 0;

	const drwav_uint64 read =
		drwav_read_pcm_frames_s16(
			&m_impl->wav,
			static_cast<drwav_uint64>(frameCount),
			dest
		);

	return static_cast<size_t>(read);
}

bool WavStreamDecoder::seek(U64 frameOffset) {
	if (!m_impl->open)
		return false;

	const drwav_bool32 ok =
		drwav_seek_to_pcm_frame(
			&m_impl->wav,
			static_cast<drwav_uint64>(frameOffset)
		);

	if (ok != DRWAV_TRUE) {
		BT_WARN(
			"WavStreamDecoder: seek to frame {} failed",
			frameOffset
		);

		return false;
	}

	return true;
}

void WavStreamDecoder::close() {
	if (m_impl->open) {
		drwav_uninit(&m_impl->wav);
		m_impl->open = false;
	}

	m_impl->metadata = {};
}

AudioMetadata WavStreamDecoder::info() const {
	return m_impl->metadata;
}

bool WavStreamDecoder::isOpen() const {
	return m_impl->open;
}

} // Blackthorn::Audio