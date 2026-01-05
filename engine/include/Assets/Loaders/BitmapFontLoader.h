#pragma once

#include "Assets/IAssetLoader.h"
#include "Fonts/BitmapFont.h"
#include "Graphics/Renderer.h"

namespace Blackthorn::Fonts {

struct BLACKTHORN_API BitmapParams : Assets::LoadParams {
	std::string texturePath;
	std::string metricsPath;

	BitmapParams(const std::string& texture, const std::string& metrics)
		: texturePath(texture)
		, metricsPath(metrics)
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<BitmapParams>(*this);
	}
};

class BitmapFontLoader : public Assets::IAssetLoader<BitmapFont> {
private:
	Graphics::Renderer* renderer;

public:
	BitmapFontLoader(Graphics::Renderer* ren)
		: renderer(ren)
	{}

	std::unique_ptr<BitmapFont> load(const Assets::LoadParams& params) override {
		const auto& p = static_cast<const BitmapParams&>(params);
		std::unique_ptr<BitmapFont> font = std::make_unique<BitmapFont>(renderer);
		font->loadFromFile(p.texturePath, p.metricsPath);
		return font;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".bmf", ".fnt" };
	}
};

} // namespace Blackthorn