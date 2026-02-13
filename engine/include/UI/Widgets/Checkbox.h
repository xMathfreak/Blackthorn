#pragma once

#include <functional>

#include "UI/Widget.h"

namespace Blackthorn::UI {

class Checkbox : public Widget {
public:
	using ChangeCallback = std::function<void()>;

	Checkbox(const std::string& text);
	~Checkbox() override = default;

	void render(Graphics::Renderer& renderer) override;
	glm::vec2 getMinimumSize() const override;

	bool onMouseUp(const glm::vec2& position, Uint8 button) override;
	bool canFocus() const override { return true; }

	void setChecked(bool checked);
	bool getChecked() const { return checked; }

	void setOnChangeCallback(ChangeCallback callback) { onChange = callback; }

	void setBoxColor(glm::vec4& color) { boxColor = color; }
	void setCheckColor(const glm::vec4& color) { checkColor = color; }
	void setBoxSize(float size) { boxSize = size; };

private:
	bool checked = false;

	glm::vec4 boxColor{0.0f, 0.0f, 0.0f, 1.0f};
	glm::vec4 checkColor{1.0f, 1.0f, 1.0f, 1.0f};
	float boxSize = 1.0f;

	ChangeCallback onChange;
};

} // namespace Blackthorn::UI