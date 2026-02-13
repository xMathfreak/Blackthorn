#pragma once

#include "Core/Export.h"
#include "UI/Container.h"

namespace Blackthorn::UI {

class BLACKTHORN_API Panel : public Container {
public:
	Panel();
	~Panel() override = default;

	void render(Graphics::Renderer& renderer) override;

	void setBackgroundColor(glm::vec4& color) {
		backgroundColor = color;
	}

	void setBorderColor(glm::vec4& color) {
		borderColor = color;
	}

	void setBorderWidth(float w) {
		borderWidth = w;
	}

	void setCornerRadius(float radius) {
		cornerRadius = radius;
	}

private:
	glm::vec4 backgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
	glm::vec4 borderColor{0.0f, 0.0f, 0.0f, 1.0f};
	float borderWidth = 0.1f;
	float cornerRadius = 0.0f;
};

} // namespace Blackthorn::UI