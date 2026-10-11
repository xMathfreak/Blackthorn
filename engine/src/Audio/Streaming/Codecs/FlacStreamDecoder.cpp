#include "Audio/Streaming/Codecs/FlacStreamDecoder.h"

#include <dr_flac.h>

#include "Audio/Streaming/Codecs/StreamCallbacks.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

namespace {

inline std::optional<StreamCallbacks::SeekOrigin> toSeekOrigin(
	drflac_seek_origin origin
) {
	switch (origin) {
		case DRFLAC_SEEK_SET:
			return StreamCallbacks::SeekOrigin::Begin;
		case DRFLAC_SEEK_CUR:
			return StreamCallbacks::SeekOrigin::Current;
		case DRFLAC_SEEK_END:
			return StreamCallbacks::SeekOrigin::End;
		default:
			return std::nullopt;
	}
}

drflac_bool32 onFlacSeek(void* user, int offset, drflac_seek_origin origin) {
	auto mapped = toSeekOrigin(origin);
	if (!mapped)
		return DRFLAC_FALSE;
	return StreamCallbacks::seek(user, offset, *mapped) ? DRFLAC_TRUE : DRFLAC_FALSE;
}

size_t onFlacRead(void* user, void* dst, size_t bytes) {
	return StreamCallbacks::read(user, dst, bytes);
}

drflac_bool32 onFlacTell(void* user, drflac_int64* cursor) {
	auto* stream = static_cast<Assets::AssetStream*>(user);
	*cursor = static_cast<drflac_int64>(stream->tell());
	return DRFLAC_TRUE;
}

} // anonymous namespace

struct FlacStreamDecoder::Impl {
	drflac* flac = nullptr;

	std::unique_ptr<Assets::AssetStream> stream; ///< Owned source for openStream(); null for path/memory opens.

	bool open = false;

	AudioMetadata metadata{};
};

FlacStreamDecoder::FlacStreamDecoder()
	: m_impl(std::make_unique<Impl>()) {}

FlacStreamDecoder::~FlacStreamDecoder() {
	close();
}

bool FlacStreamDecoder::open(
	const std::filesystem::path& path
) {
	close();

	const auto pathStr = path.string();

	m_impl->flac =
		drflac_open_file(pathStr.c_str(), nullptr);

	if (!m_impl->flac) {
		BT_ERROR(
			"FlacStreamDecoder: failed to open '{}'",
			pathStr
		);

		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels =
		static_cast<U32>(m_impl->flac->channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(m_impl->flac->sampleRate);

	m_impl->metadata.frameCount =
		static_cast<U64>(m_impl->flac->totalPCMFrameCount);

	return true;
}

bool FlacStreamDecoder::openMemory(const U8* data, size_t size) {
	close();

	m_impl->flac = drflac_open_memory(data, size, nullptr);

	if (!m_impl->flac) {
		BT_ERROR(
			"FlacStreamDecoder: failed to open from memory ({} bytes)",
			size
		);

		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels =
		static_cast<U32>(m_impl->flac->channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(m_impl->flac->sampleRate);

	m_impl->metadata.frameCount =
		static_cast<U64>(m_impl->flac->totalPCMFrameCount);

	return true;
}

bool FlacStreamDecoder::openStream(std::unique_ptr<Assets::AssetStream> stream) {
	close();

	if (!stream) {
		BT_ERROR("FlacStreamDecoder: openStream called with a null stream");
		return false;
	}

	m_impl->stream = std::move(stream);

	m_impl->flac = drflac_open(onFlacRead, onFlacSeek, onFlacTell, m_impl->stream.get(), nullptr);

	if (!m_impl->flac) {
		BT_ERROR("FlacStreamDecoder: failed to open stream ({} bytes)", m_impl->stream->size());
		m_impl->stream.reset();
		return false;
	}

	m_impl->open = true;

	m_impl->metadata.channels = static_cast<U32>(m_impl->flac->channels);
	m_impl->metadata.sampleRate = static_cast<U32>(m_impl->flac->sampleRate);
	m_impl->metadata.frameCount = static_cast<U64>(m_impl->flac->totalPCMFrameCount);

	BT_DEBUG(
		"FlacStreamDecoder: opened stream ({} bytes) [{} ch, {} Hz, {} frames]",
		m_impl->stream->size(),
		m_impl->metadata.channels,
		m_impl->metadata.sampleRate,
		m_impl->metadata.frameCount
	);

	return true;
}

size_t FlacStreamDecoder::readFrames(
	I16* dest,
	size_t frameCount
) {
	if (!m_impl->open)
		return 0;

	const drflac_uint64 read =
		drflac_read_pcm_frames_s16(
			m_impl->flac,
			static_cast<drflac_uint64>(frameCount),
			dest
		);

	return static_cast<size_t>(read);
}

bool FlacStreamDecoder::seek(U64 frameOffset) {
	if (!m_impl->open)
		return false;

	const drflac_bool32 ok =
		drflac_seek_to_pcm_frame(
			m_impl->flac,
			static_cast<drflac_uint64>(frameOffset)
		);

	if (ok == DRFLAC_FALSE) {
		BT_WARN(
			"FlacStreamDecoder: seek failed ({})",
			frameOffset
		);

		return false;
	}

	return true;
}

void FlacStreamDecoder::close() {
	if (m_impl->flac) {
		drflac_close(m_impl->flac);
		m_impl->flac = nullptr;
	}

	// Freed after drflac_close(), since the decoder may still read through it until then.
	m_impl->stream.reset();

	m_impl->open = false;
	m_impl->metadata = {};
}

AudioMetadata FlacStreamDecoder::info() const {
	return m_impl->metadata;
}

bool FlacStreamDecoder::isOpen() const {
	return m_impl->open;
}

} // namespace Blackthorn::Audio::Streaming