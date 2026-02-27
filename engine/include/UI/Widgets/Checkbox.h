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

class BLACKTHORN_API Checkbox : public Widget {
public:
	using ChangeCallback = std::function<void(bool)>;

	Checkbox(bool active = false);
	~Checkbox() override = default;

	void render(Graphics::Renderer& renderer) override;
	glm::vec2 getMinimumSize() const override;

	bool onMouseUp(const glm::vec2& position, Uint8 button) override;
	bool canFocus() const override { return true; }

	void setChecked(bool checked);
	bool getChecked() const { return checked; }

	void setOnChangeCallback(ChangeCallback callback) { onChange = callback; }

	void setBoxColor(const Math::Color& color) { boxColor = color; }
	void setCheckColor(const Math::Color& color) { checkColor = color; }
	void setBoxSize(float s) { boxSize = s; };

	void setBackgroundTexture(Graphics::Texture* tex) { backgroundTexture = tex; }
	void setCheckmarkTexture(Graphics::Texture* tex) { checkmarkTexture = tex; }

	void setUseNineSlice(bool use) { useNineSlice = use; }
	void setNineSliceMargins(float left, float top, float right, float bottom) {
		nineSliceMargins = {left, top, right, bottom};
		useNineSlice = true;
	}

	void setCheckmarkPadding(float pad) { checkmarkPadding = pad; }
	void setCheckmarkScale(float scale) { checkmarkScale = scale; }

private:
	bool checked = false;

	Math::Color boxColor = Math::Colors::Black;
	Math::Color checkColor = Math::Colors::White;
	float boxSize = 1.0f;

	Graphics::Texture* backgroundTexture = nullptr;
	Graphics::Texture* checkmarkTexture = nullptr;

	SDL_FRect nineSliceMargins{0, 0, 0, 0};
	bool useNineSlice = false;

	float checkmarkPadding = 0.0f;
	float checkmarkScale = 1.0f;

	ChangeCallback onChange;
};

} // namespace UI

} // namespace Blackthorn