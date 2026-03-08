#pragma once

#include <string>

#include "Core/Export.h"
#include "Fonts/TextTypes.h"
#include "Math/Color.h"
#include "UI/Widget.h"

namespace Blackthorn {

namespace Fonts {
	class Font;
}

namespace UI {

class BLACKTHORN_API Label : public Widget {
public:
	enum class Mode : Uint8 {
		Dynamic,
		Static
	};

protected:
	std::string text;
	Fonts::Font* font = nullptr;
	Math::Color textColor = Math::Colors::White;
	float textScale = 1.0f;
	Mode renderMode = Mode::Dynamic;
	Text::Alignment textAlignment = Text::Alignment::Left;

	mutable glm::vec2 cachedTextSize{0};
	mutable bool textSizeDirty = true;

	void updateTextSize() const;

public:
	Label(const std::string& text = "", Mode mode = Mode::Dynamic);
	~Label() override = default;

	void render(Graphics::Renderer& renderer) override;

	glm::vec2 getMinimumSize() const override;
	void updateLayout() override;

	void setText(const std::string& t);
	const std::string& getText() const { return text; }

	void setTextColor(const Math::Color& color);
	const Math::Color& getTextColor() const { return textColor; }

	void setFont(Fonts::Font* f);
	Fonts::Font* getFont() const { return font; }

	void setScale(float scale);
	float getScale() const { return textScale; }

	void setTextAlignment(Text::Alignment align);
	Text::Alignment getTextAlignment() const { return textAlignment; }

	void setMode(Mode mode);
	Mode getMode() const { return renderMode; }

	void markLayoutDirty() override;
};

} // namespace UI

} // namespace Blackthorn