#pragma once

#include <filesystem>
#include <memory>

#include "Assets/AssetStream.hpp"

#include "Audio/Resources/AudioData.hpp"
#include "Audio/Streaming/IStreamDecoder.hpp"
#include "Core/Export.hpp"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API OggStreamDecoder final : public IStreamDecoder {
public:
	OggStreamDecoder();
	~OggStreamDecoder() override;

	OggStreamDecoder(const OggStreamDecoder&) = delete;
	OggStreamDecoder& operator=(const OggStreamDecoder&) = delete;

	bool open(const std::filesystem::path& path) override;
	bool openMemory(const U8* data, size_t size) override;
	bool openStream(std::unique_ptr<Assets::AssetStream> stream) override;
	size_t readFrames(I16* dest, size_t frameCount) override;
	bool seek(U64 frameOffset) override;
	void close() override;

	AudioMetadata info() const override;
	bool isOpen() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Blackthorn::Audio