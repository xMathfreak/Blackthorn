#pragma once

#include <vector>

#include "Assets/RawAssetData.h"
#include "Graphics/Texture.h"
#include "Core/Export.h"

namespace Blackthorn::Graphics {

struct BLACKTHORN_API RawTextureData : Assets::IRawAssetData {
	std::vector<Uint8> pixels;

	int width = 0;
	int height = 0;
	int channels = 0;

	TextureParams params;

	RawTextureData() = default;
	explicit RawTextureData(std::string id) : IRawAssetData(std::move(id)) {}
};

} // namespace Blackthorn::Graphics
