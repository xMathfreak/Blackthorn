#pragma once

#include "Assets/IAssetLoader.h"
#include "Graphics/Texture.h"

namespace Blackthorn::Graphics {

class TextureLoader : public Assets::IAssetLoader<Texture> {
public:
	std::unique_ptr<Graphics::Texture> load(const Assets::LoadParams& params) override {
		const auto& path = static_cast<const Assets::PathLoadParams&>(params).path;
		return std::make_unique<Texture>(path);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".png", ".bmp", ".jpg", ".jpeg", ".tga" };
	}
};

} // namespace Blackthorn::Graphics