#pragma once

#include <filesystem>
#include <string>

#include "Audio/Resources/AudioData.h"
#include "Audio/Streaming/IStreamDecoder.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API Mp3StreamDecoder final : public IStreamDecoder {
public:
	Mp3StreamDecoder();
	~Mp3StreamDecoder() override;

	Mp3StreamDecoder(const Mp3StreamDecoder&) = delete;
	Mp3StreamDecoder& operator=(const Mp3StreamDecoder&) = delete;

	bool open(const std::filesystem::path& path) override;
	bool openMemory(const U8* data, size_t size) override;
	size_t readFrames(I16* dest, size_t frameCount) override;
	bool seek(U64 frameOffset) override;
	void close() override;

	AudioMetadata info() const override;
	bool isOpen() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	/// Shared by open()/openMemory() after drmp3_init_file()/init_memory()
	/// succeeds: builds the seek table and reads metadata. sourceLabel is
	/// used in log messages only.
	bool finalizeAfterInit(const std::string& sourceLabel);
};

} // namespace Blackthorn::Audio