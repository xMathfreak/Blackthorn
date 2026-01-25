#pragma once

#include "Assets/IAssetLoader.h"
#include "Fonts/BitmapFont.h"

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
public:
	std::unique_ptr<BitmapFont> load(const Assets::LoadParams& params) override {
		std::unique_ptr<BitmapFont> font = std::make_unique<BitmapFont>();
		if (const BitmapParams* splitParams = dynamic_cast<const BitmapParams*>(&params)) {
			font->loadFromFile(splitParams->texturePath, splitParams->metricsPath);
			return font;
		} else if (const auto* binaryParams = dynamic_cast<const Assets::PathLoadParams*>(&params)) {
			font->loadFromBMFont(binaryParams->path);
			return font;
		}

		font.reset();
		return nullptr;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".bmf", ".fnt" };
	}
};

} // namespace Blackthorn