#pragma once

#include <filesystem>
#include <memory>

#include "Audio/Streaming/IStreamDecoder.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio::Streaming {

/**
 * @brief Factory that constructs the correct @c IStreamDecoder for a given
 *        audio file path.
 *
 *        Format dispatch is done by file extension (case-insensitive).
 *        Supported extensions and their concrete decoders:
 *
 * | Extension | Decoder            | Backend      |
 * |-----------|--------------------|--------------|
 * | .wav      | WavStreamDecoder   | dr_wav       |
 * | .mp3      | Mp3StreamDecoder   | dr_mp3       |
 * | .flac     | FlacStreamDecoder  | dr_flac      |
 * | .ogg      | OggStreamDecoder   | libvorbisfile|
 *
 * @code
 * auto decoder = StreamDecoderFactory::create("assets/music.ogg");
 * if (!decoder || !decoder->open("assets/music.ogg")) {
 *     BT_ERROR("Failed to open stream");
 * }
 * @endcode
 */
class BLACKTHORN_API StreamDecoderFactory {
public:
	StreamDecoderFactory() = delete;

	/**
	 * @brief Creates an @c IStreamDecoder appropriate for @p path.
	 *
	 * @note Does not call @c open(). The caller is responsible for opening the
	 * decoder before reading.
	 *
	 * @param path Path to the audio file. Only the extension is inspected;
	 *             the file does not need to exist at call time.
	 * @return     A concrete decoder, or @c nullptr if the extension is
	 *             unsupported.
	 */
	static std::unique_ptr<IStreamDecoder> create(const std::filesystem::path& path);

	/**
	 * @brief Creates an @c IStreamDecoder appropriate for an in-memory buffer.
	 *
	 * Alternative to create() for packed/in-memory assets that have no file
	 * extension to dispatch on. The container format is instead detected
	 * from the first bytes of @p data via magic-number inspection (see
	 * Decoding::AudioDecoder::detectFormat()).
	 *
	 * @note Does not call openMemory(). The caller is responsible for
	 * opening the decoder before reading, and must keep @p data alive for as
	 * long as the returned decoder remains open (see
	 * IStreamDecoder::openMemory()).
	 *
	 * @param data Pointer to the encoded audio bytes.
	 * @param size Size of @p data in bytes.
	 * @return A concrete decoder, or @c nullptr if the format is unrecognised.
	 */
	static std::unique_ptr<IStreamDecoder> createFromMemory(const U8* data, size_t size);

	/**
	 * @brief Returns true if @p path has a supported audio extension.
	 */
	static bool isSupported(const std::filesystem::path& path) noexcept;
};

} // namespace Blackthorn::Audio