#pragma once

#include <functional>

#include "Core/Export.h"
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

	void setNormalColor(const glm::vec4& color) { normalColor = color; }
	void setHoverColor(const glm::vec4& color) { hoverColor = color; }
	void setPressedColor(const glm::vec4& color) { pressedColor = color; }
	void setTextColor(const glm::vec4& color) { textColor = color; }

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

	glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
	glm::vec4 hoverColor{0.0f, 0.0f, 0.0f, 1.0f};
	glm::vec4 pressedColor{0.0f, 0.0f, 0.0f, 1.0f};
	glm::vec4 normalColor{0.0f, 0.0f, 0.0f, 1.0f};

	ClickCallback onClick;
};

} // namespace UI

} // namespace Blackthorn