#pragma once

#include <functional>

#include "Core/Export.h"
#include "Math/Color.h"
#include "UI/Widget.h"

namespace Blackthorn {

namespace Graphics {
	class Texture;
}

namespace UI {

class BLACKTHORN_API Button : public Widget {
public:
	using ClickCallback = std::function<void()>;

	Button(const std::string& text);
	~Button() override = default;

	void render(Graphics::Renderer& renderer) override;
	glm::vec2 getMinimumSize() const override;

	bool onMouseUp(const glm::vec2& position, Uint8 button) override;
	bool canFocus() const override { return true; }

	void setText(const std::string& t);
	const std::string& getText() const { return text; }

	void setFont(Fonts::Font* f) { font = f; }
	void setScale(float scale) { textScale = scale; }

	void setOnClickCallback(ClickCallback callback) { onClick = callback; }

	void setNormalColor(const Math::Color& color) { normalColor = color; }
	void setHoverColor(const Math::Color& color) { hoverColor = color; }
	void setPressedColor(const Math::Color& color) { pressedColor = color; }
	void setTextColor(const Math::Color& color) { textColor = color; }

	void setUseNineSlice(bool use) { useNineSlice = use; }
	void setBackgroundTexture(Graphics::Texture* tex) { backgroundTexture = tex; }
	void setNineSliceMargins(float left, float top, float right, float bottom) {
		nineSliceMargins = {left, top, right, bottom};
		useNineSlice = true;
	}

private:
	std::string text;
	Fonts::Font* font = nullptr;
	float textScale = 1.0f;

	bool useNineSlice = false;
	SDL_FRect nineSliceMargins{0, 0, 0, 0};
	Graphics::Texture* backgroundTexture = nullptr;

	Math::Color textColor = Math::Colors::White;
	Math::Color hoverColor = Math::Colors::Black;
	Math::Color pressedColor = Math::Colors::Black;
	Math::Color normalColor = Math::Colors::Black;

	ClickCallback onClick;
};

} // namespace UI

} // namespace Blackthorn