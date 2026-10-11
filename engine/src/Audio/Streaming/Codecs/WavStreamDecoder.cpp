#include "Audio/Streaming/Codecs/WavStreamDecoder.h"

#include <dr_wav.h>

#include "Audio/Streaming/Codecs/StreamCallbacks.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

struct WavStreamDecoder::Impl {
	drwav wav{};

	std::unique_ptr<Assets::AssetStream> stream; ///< Owned source for openStream(); null for path/memory opens.

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

bool WavStreamDecoder::openMemory(const U8* data, size_t size) {
	close();

	if (!drwav_init_memory(&m_impl->wav, data, size, nullptr)) {
		BT_ERROR(
			"WavStreamDecoder: failed to open from memory ({} bytes)",
			size
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
		"WavStreamDecoder: opened from memory ({} bytes) [{} ch, {} Hz, {} frames]",
		size,
		m_impl->metadata.channels,
		m_impl->metadata.sampleRate,
		m_impl->metadata.frameCount
	);

	return true;
}

namespace {

inline std::optional<StreamCallbacks::SeekOrigin> toSeekOrigin(
	drwav_seek_origin origin
) {
	switch (origin) {
		case DRWAV_SEEK_SET:
			return StreamCallbacks::SeekOrigin::Begin;
		case DRWAV_SEEK_CUR:
			return StreamCallbacks::SeekOrigin::Current;
		case DRWAV_SEEK_END:
			return StreamCallbacks::SeekOrigin::End;
		default:
			return std::nullopt;
	}
}

size_t onWavRead(void* user, void* dst, size_t bytes) {
	return StreamCallbacks::read(user, dst, bytes);
}

drwav_bool32 onWavSeek(void* user, int offset, drwav_seek_origin origin) {
	auto mapped = toSeekOrigin(origin);
	if (!mapped)
		return DRWAV_FALSE;
	return StreamCallbacks::seek(user, offset, *mapped) ? DRWAV_TRUE : DRWAV_FALSE;
}

drwav_bool32 onWavTell(void* user, drwav_int64* cursor) {
	auto* stream = static_cast<Assets::AssetStream*>(user);
	*cursor = static_cast<drwav_int64>(stream->tell());
	return DRWAV_TRUE;
}

} // namespace

bool WavStreamDecoder::openStream(std::unique_ptr<Assets::AssetStream> stream) {
	close();

	if (!stream) {
		BT_ERROR("WavStreamDecoder: openStream called with a null stream");
		return false;
	}

	m_impl->stream = std::move(stream);

	if (!drwav_init(&m_impl->wav, onWavRead, onWavSeek, onWavTell, m_impl->stream.get(), nullptr)) {
		BT_ERROR("WavStreamDecoder: failed to open stream ({} bytes)", m_impl->stream->size());
		m_impl->stream.reset();
		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels = static_cast<U32>(m_impl->wav.channels);
	m_impl->metadata.sampleRate = static_cast<U32>(m_impl->wav.sampleRate);
	m_impl->metadata.frameCount = static_cast<U64>(m_impl->wav.totalPCMFrameCount);

	BT_DEBUG(
		"WavStreamDecoder: opened stream ({} bytes) [{} ch, {} Hz, {} frames]",
		m_impl->stream->size(),
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

	// Freed after drwav_uninit, since the decoder may still reference it until then.
	m_impl->stream.reset();

	m_impl->metadata = {};
}

AudioMetadata WavStreamDecoder::info() const {
	return m_impl->metadata;
}

bool WavStreamDecoder::isOpen() const {
	return m_impl->open;
}

} // Blackthorn::Audio::Streaming