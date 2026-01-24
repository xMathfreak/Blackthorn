#pragma once

#include "Assets/IAssetLoader.h"
#include "Fonts/TrueTypeFont.h"

namespace Blackthorn::Fonts {

struct BLACKTHORN_API TTFParams : Assets::LoadParams {
	std::string path;
	int size;

	TTFParams(const std::string& ttfPath, int pointSize)
		: path(ttfPath)
		, size(pointSize)
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<TTFParams>(*this);
	}
};

class TrueTypeFontLoader : public Assets::IAssetLoader<TrueTypeFont> {
public:
	std::unique_ptr<TrueTypeFont> load(const Assets::LoadParams& params) override {
		const auto& p = static_cast<const TTFParams&>(params);
		std::unique_ptr<TrueTypeFont> font = std::make_unique<TrueTypeFont>();
		font->loadFromFile(p.path, p.size);
		return font;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".ttf", ".otf"};
	}
};

} // namespace Blackthorn::Fonts