#pragma once

#include <string>

#include "Core/Export.h"
#include "Math/Color.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

class BLACKTHORN_API Label : public Widget {
public:
	enum class Mode {
		Dynamic,
		Static
	};

public:
	Label(const std::string& text = "", Mode mode = Mode::Dynamic);
	~Label() override = default;

	void render(Graphics::Renderer& renderer) override;
	glm::vec2 getMinimumSize() const override;

	void setText(const std::string& t);
	const std::string& getText() const { return text; }

	void setTextColor(const Math::Color& color) { textColor = color; }
	void setFont(Fonts::Font* f) { font = f; }
	void setScale(float scale) { textScale = scale; }

private:
	std::string text;
	Fonts::Font* font = nullptr;
	Math::Color textColor = Math::Colors::White;
	float textScale = 1.0f;
	Mode renderMode = Mode::Dynamic;
};

} // namespace Blackthorn::UI