#include "Audio/Decoding/Codecs/OggDecoder.h"

#include <cstring>

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
		static_cast<size_t>(ov_pcm_total(&vf, -1)) * info->channels;

	data.samples.resize(sampleCount);

	size_t samplesRead = 0;
	int bitstream = 0;

	while (samplesRead < sampleCount) {
		const size_t remaining = sampleCount - samplesRead;

		long ret = ov_read(
			&vf,
			reinterpret_cast<char*>(data.samples.data() + samplesRead),
			static_cast<int>(remaining * sizeof(I16)),
			0, // little-endian
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
	const std::filesystem::path& path,
	AudioMetadata& data
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
	data.frameCount = static_cast<U64>(ov_pcm_total(&vf, -1));

	ov_clear(&vf);
	return true;
}

namespace {

/**
 * @brief Cursor struct used as the datasource for ov_open_callbacks().
 */
struct OggMemoryCursor {
	const U8* data; ///< Pointer to the encoded OGG bytes.
	size_t size; ///< Total byte size of the buffer.
	size_t pos; ///< Current read position (byte offset).
};

/**
 * @brief Copies up to nmemb * size bytes into ptr.
 */
size_t oggMemRead(void* ptr, size_t blockSize, size_t nmemb, void* datasource) {
	auto* cursor = static_cast<OggMemoryCursor*>(datasource);
	const size_t remaining = cursor->size - cursor->pos;
	const size_t requested = blockSize * nmemb;
	const size_t toRead = (requested < remaining) ? requested : remaining;

	if (toRead == 0)
		return 0;

	std::memcpy(ptr, cursor->data + cursor->pos, toRead);
	cursor->pos += toRead;

	return toRead / blockSize;
}

/**
 * @brief Moves the read cursor.
 *
 * @param offset Signed byte offset.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return 0 on success, -1 on failure (matching fseek convention).
 */
int oggMemSeek(void* datasource, ogg_int64_t offset, int whence) {
	auto* cursor = static_cast<OggMemoryCursor*>(datasource);
	ogg_int64_t newPos = 0;

	switch (whence) {
		case SEEK_SET:
			newPos = offset;
			break;
		case SEEK_CUR:
			newPos = static_cast<ogg_int64_t>(cursor->pos) + offset;
			break;
		case SEEK_END:
			newPos = static_cast<ogg_int64_t>(cursor->size) + offset;
			break;
		default:
			return -1;
	}

	if (newPos < 0 || static_cast<size_t>(newPos) > cursor->size)
		return -1;

	cursor->pos = static_cast<size_t>(newPos);
	return 0;
}

/**
 * @brief ov_callbacks::close_func no-op.
 */
int oggMemClose(void* /*datasource*/) {
	return 0;
}

/**
 * @brief Returns the current byte position.
 */
long oggMemTell(void* datasource) {
	const auto* cursor = static_cast<const OggMemoryCursor*>(datasource);
	return static_cast<long>(cursor->pos);
}

/// Callback table backed by OggMemoryCursor.
constexpr ov_callbacks k_OggMemCallbacks = {
	oggMemRead,
	oggMemSeek,
	oggMemClose,
	oggMemTell,
};

} // anonymous namespace

bool OggDecoder::decodeFromMemory(
	const U8* src,
	size_t srcSize,
	AudioData& data
) {
	OggMemoryCursor cursor{ src, srcSize, 0 };

	OggVorbis_File vf;

	if (ov_open_callbacks(&cursor, &vf, nullptr, 0, k_OggMemCallbacks) != 0) {
		BT_ERROR("OggDecoder: ov_open_callbacks failed for memory buffer");
		return false;
	}

	const vorbis_info* info = ov_info(&vf, -1);
	if (!info) {
		ov_clear(&vf);
		BT_ERROR("OggDecoder: ov_info returned null for memory buffer");
		return false;
	}

	const size_t sampleCount =
		static_cast<size_t>(ov_pcm_total(&vf, -1)) * info->channels;

	data.samples.resize(sampleCount);

	size_t samplesRead = 0;
	int bitstream = 0;

	while (samplesRead < sampleCount) {
		const size_t remaining = sampleCount - samplesRead;

		long ret = ov_read(
			&vf,
			reinterpret_cast<char*>(data.samples.data() + samplesRead),
			static_cast<int>(remaining * sizeof(I16)),
			0, // little-endian
			sizeof(I16), // 16-bit
			1, // signed
			&bitstream
		);

		if (ret == 0)
			break;

		if (ret < 0) {
			BT_WARN("OggDecoder: ov_read error {} (memory buffer)", ret);
			break;
		}

		samplesRead += static_cast<size_t>(ret) / sizeof(I16);
	}

	ov_clear(&vf);
	return samplesRead > 0;
}

bool OggDecoder::getInfoFromMemory(
	const U8* src,
	size_t srcSize,
	AudioMetadata& data
) {
	OggMemoryCursor cursor{ src, srcSize, 0 };

	OggVorbis_File vf;
	if (ov_open_callbacks(&cursor, &vf, nullptr, 0, k_OggMemCallbacks) != 0) {
		BT_ERROR("OggDecoder: ov_open_callbacks failed for memory buffer (getInfo)");
		return false;
	}

	const vorbis_info* info = ov_info(&vf, -1);
	if (!info) {
		ov_clear(&vf);
		return false;
	}

	data.channels = static_cast<U32>(info->channels);
	data.sampleRate = static_cast<U32>(info->rate);
	data.frameCount = static_cast<U64>(ov_pcm_total(&vf, -1));

	ov_clear(&vf);
	return true;
}

} // namespace Blackthorn::Audio::Decoding