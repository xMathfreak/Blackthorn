#include "Audio/Streaming/Codecs/OggStreamDecoder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <vorbis/vorbisfile.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

namespace {

/**
 * @brief Backing store for openMemory(). libvorbisfile has no built-in
 * memory-open function (unlike dr_wav/dr_mp3/dr_flac), so it's opened
 * via ov_open_callbacks() with these four callbacks reading from a
 * plain in-memory buffer instead of a FILE*.
 *
 * Does not own @c data, the caller (AudioClip's owned compressed-bytes
 * buffer) must outlive the OggVorbis_File, exactly as documented on
 * IStreamDecoder::openMemory().
 */
struct MemorySource {
	const unsigned char* data = nullptr;
	size_t size = 0;
	size_t pos = 0;
};

size_t memRead(void* ptr, size_t size, size_t nmemb, void* datasource) {
	auto* src = static_cast<MemorySource*>(datasource);

	const size_t bytesRequested = size * nmemb;
	const size_t bytesAvailable = src->size - src->pos;
	const size_t bytesToRead = std::min(bytesRequested, bytesAvailable);

	if (bytesToRead > 0) {
		std::memcpy(ptr, src->data + src->pos, bytesToRead);
		src->pos += bytesToRead;
	}

	return (size > 0) ? (bytesToRead / size) : 0;
}

int memSeek(void* datasource, ogg_int64_t offset, int whence) {
	auto* src = static_cast<MemorySource*>(datasource);

	I64 target = 0;

	switch (whence) {
		case SEEK_SET:
			target = offset;
			break;
		case SEEK_CUR:
			target = static_cast<I64>(src->pos) + offset;
			break;
		case SEEK_END:
			target = static_cast<I64>(src->size) + offset;
			break;
		default:
			return -1;
	}

	if (target < 0 || static_cast<size_t>(target) > src->size)
		return -1;

	src->pos = static_cast<size_t>(target);
	return 0;
}

int memClose(void* /*datasource*/) {
	return 0;
}

long memTell(void* datasource) {
	auto* src = static_cast<MemorySource*>(datasource);
	return static_cast<long>(src->pos);
}

} // anonymous namespace

struct OggStreamDecoder::Impl {
	OggVorbis_File vf{};

	bool open = false;

	AudioMetadata metadata{};
	MemorySource memSource;
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

bool OggStreamDecoder::openMemory(const U8* data, size_t size) {
	close();

	m_impl->memSource.data = data;
	m_impl->memSource.size = size;
	m_impl->memSource.pos = 0;

	ov_callbacks callbacks{};
	callbacks.read_func = memRead;
	callbacks.seek_func = memSeek;
	callbacks.close_func = memClose;
	callbacks.tell_func = memTell;

	if (ov_open_callbacks(&m_impl->memSource, &m_impl->vf, nullptr, 0, callbacks) != 0) {
		BT_ERROR(
			"OggStreamDecoder: failed to open from memory ({} bytes)",
			size
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
	m_impl->memSource = {};
}

AudioMetadata OggStreamDecoder::info() const {
	return m_impl->metadata;
}

bool OggStreamDecoder::isOpen() const {
	return m_impl->open;
}

} // namespace Blackthorn::Audio::Streaming