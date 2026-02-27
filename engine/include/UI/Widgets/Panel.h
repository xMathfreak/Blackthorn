#pragma once

#include "Core/Export.h"
#include "Math/Color.h"
#include "UI/Container.h"

namespace Blackthorn::UI {

class BLACKTHORN_API Panel : public Container {
public:
	Panel();
	~Panel() override = default;

	void render(Graphics::Renderer& renderer) override;

	void setBackgroundColor(const Math::Color& color) {
		backgroundColor = color;
	}

	void setBorderColor(const Math::Color& color) {
		borderColor = color;
	}

	void setBorderWidth(float w) {
		borderWidth = w;
	}

	void setCornerRadius(float radius) {
		cornerRadius = radius;
	}

	void layoutChildren() override;

	void setAutoResize(bool resizing) { autoResize = resizing; }
	bool isAutoResize() const { return autoResize; }

private:
	Math::Color backgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
	Math::Color borderColor{0.0f, 0.0f, 0.0f, 1.0f};
	float borderWidth = 0.1f;
	float cornerRadius = 0.0f;
	bool autoResize = true;
};

} // namespace Blackthorn::UI