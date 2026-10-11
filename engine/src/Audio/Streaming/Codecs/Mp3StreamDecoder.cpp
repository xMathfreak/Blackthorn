#include "Audio/Streaming/Codecs/Mp3StreamDecoder.h"

#include <dr_mp3.h>

#include "Audio/Streaming/Codecs/StreamCallbacks.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

namespace {

inline std::optional<StreamCallbacks::SeekOrigin> toSeekOrigin(
	drmp3_seek_origin origin
) {
	switch (origin) {
		case DRMP3_SEEK_SET:
			return StreamCallbacks::SeekOrigin::Begin;
		case DRMP3_SEEK_CUR:
			return StreamCallbacks::SeekOrigin::Current;
		case DRMP3_SEEK_END:
			return StreamCallbacks::SeekOrigin::End;
		default:
			return std::nullopt;
	}
}

drmp3_bool32 onMp3Seek(void* user, int offset, drmp3_seek_origin origin) {
	auto mapped = toSeekOrigin(origin);
	if (!mapped)
		return DRMP3_FALSE;
	return StreamCallbacks::seek(user, offset, *mapped) ? DRMP3_TRUE : DRMP3_FALSE;
}

size_t onMp3Read(void* user, void* dst, size_t bytes) {
	return StreamCallbacks::read(user, dst, bytes);
}

drmp3_bool32 onMp3Tell(void* user, drmp3_int64* cursor) {
	auto* stream = static_cast<Assets::AssetStream*>(user);
	*cursor = static_cast<drmp3_int64>(stream->tell());
	return DRMP3_TRUE;
}

} // anonymous namespace

struct Mp3StreamDecoder::Impl {
	drmp3 mp3{};

	std::unique_ptr<Assets::AssetStream> stream; ///< Owned source for openStream(); null for path/memory opens.

	std::vector<drmp3_seek_point> seekPoints;

	bool open = false;

	AudioMetadata metadata{};
};

static constexpr drmp3_uint32 SEEK_POINT_COUNT = 128;

Mp3StreamDecoder::Mp3StreamDecoder()
	: m_impl(std::make_unique<Impl>()) {}

Mp3StreamDecoder::~Mp3StreamDecoder() {
	close();
}

bool Mp3StreamDecoder::open(const std::filesystem::path& path) {
	close();

	const auto pathStr = path.string();

	if (!drmp3_init_file(&m_impl->mp3, pathStr.c_str(), nullptr)) {
		BT_ERROR("Mp3StreamDecoder: failed to open '{}'", pathStr);
		return false;
	}

	return finalizeAfterInit(pathStr);
}

bool Mp3StreamDecoder::openMemory(const U8* data, size_t size) {
	close();

	if (!drmp3_init_memory(&m_impl->mp3, data, size, nullptr)) {
		BT_ERROR("Mp3StreamDecoder: failed to open from memory ({} bytes)", size);
		return false;
	}

	return finalizeAfterInit("<memory>");
}

bool Mp3StreamDecoder::openStream(std::unique_ptr<Assets::AssetStream> stream) {
	close();

	if (!stream) {
		BT_ERROR("Mp3StreamDecoder: openStream called with a null stream");
		return false;
	}

	m_impl->stream = std::move(stream);

	if (!drmp3_init(&m_impl->mp3, onMp3Read, onMp3Seek, onMp3Tell, nullptr, m_impl->stream.get(), nullptr)) {
		BT_ERROR("Mp3StreamDecoder: failed to open stream ({} bytes)", m_impl->stream->size());
		m_impl->stream.reset();
		return false;
	}

	return finalizeAfterInit("<stream>");
}

bool Mp3StreamDecoder::finalizeAfterInit(const std::string& sourceLabel) {
	m_impl->open = true;

	m_impl->metadata.channels =
		static_cast<U32>(m_impl->mp3.channels);

	m_impl->metadata.sampleRate =
		static_cast<U32>(m_impl->mp3.sampleRate);

	m_impl->metadata.frameCount =
		static_cast<U64>(m_impl->mp3.totalPCMFrameCount);

	drmp3_uint32 actualPointCount = SEEK_POINT_COUNT;
	m_impl->seekPoints.resize(SEEK_POINT_COUNT);

	const drmp3_bool32 tableBuilt = drmp3_calculate_seek_points(
		&m_impl->mp3,
		&actualPointCount,
		m_impl->seekPoints.data()
	);

	if (tableBuilt == DRMP3_TRUE && actualPointCount > 0) {
		m_impl->seekPoints.resize(actualPointCount);

		drmp3_bind_seek_table(
			&m_impl->mp3,
			actualPointCount,
			m_impl->seekPoints.data()
		);

		BT_DEBUG(
			"Mp3StreamDecoder: seek table built for '{}' "
			"({} points, {} ch, {} Hz, {} frames)",
			sourceLabel,
			actualPointCount,
			m_impl->metadata.channels,
			m_impl->metadata.sampleRate,
			m_impl->metadata.frameCount
		);
	} else {
		m_impl->seekPoints.clear();

		BT_WARN(
			"Mp3StreamDecoder: failed to build seek table for '{}' "
			", seeks will use the slower forward-scan path",
			sourceLabel
		);
	}

	drmp3_seek_to_pcm_frame(&m_impl->mp3, 0);

	return true;
}

size_t Mp3StreamDecoder::readFrames(I16* dest, size_t frameCount) {
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
		BT_WARN("Mp3StreamDecoder: seek to frame {} failed", frameOffset);
		return false;
	}

	return true;
}

void Mp3StreamDecoder::close() {
	if (m_impl->open) {
		drmp3_bind_seek_table(&m_impl->mp3, 0, nullptr);
		drmp3_uninit(&m_impl->mp3);
		m_impl->open = false;
	}

	// Freed after drmp3_uninit(), since the decoder may still read through it until then.
	m_impl->stream.reset();

	m_impl->seekPoints.clear();
	m_impl->metadata = {};
}

AudioMetadata Mp3StreamDecoder::info() const {
	return m_impl->metadata;
}

bool Mp3StreamDecoder::isOpen() const {
	return m_impl->open;
}

} // namespace Blackthorn::Audio::Streaming