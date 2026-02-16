#include "UI/Widgets/Checkbox.h"
#include "Graphics/Renderer.h"

namespace Blackthorn::UI {

Checkbox::Checkbox(bool active)
	: checked(active)
{
	size = glm::vec2{boxSize};
}

bool Checkbox::onMouseUp(const glm::vec2& pos, Uint8 button) {
	bool handled = Widget::onMouseUp(position, button);

	if (handled && containsPoint(pos))
		setChecked(!checked);

	return handled;
}

glm::vec2 Checkbox::getMinimumSize() const {
	if (backgroundTexture && useNineSlice) {
		float minW = nineSliceMargins.x + nineSliceMargins.w;
		float minH = nineSliceMargins.y + nineSliceMargins.h;

		return glm::vec2{minW, minH};
	}

	return glm::vec2{boxSize};
}

void Checkbox::setChecked(bool active) {
	if (checked != active) {
		checked = active;

		if (onChange) {
			onChange(checked);
		}
	}
}

void Checkbox::render(Graphics::Renderer& renderer) {
	if (!isVisible())
		return;

	glm::vec2 absPos = getAbsolutePosition();
	SDL_FRect rect = {absPos.x, absPos.y, size.x, size.y};

	glm::vec4 bgTint{1.0f};
	glm::vec4 checkTint{1.0f};

	if (hasState(state, WidgetState::Disabled)) {
		bgTint = glm::vec4{0.5f, 0.5f, 0.5f, 1.0f};
		checkTint = glm::vec4{0.5f, 0.5f, 0.5f, 1.0f};
	} else if (hasState(state, WidgetState::Pressed)) {
		bgTint = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f};
	} else if (hasState(state, WidgetState::Hovered)) {
		bgTint = glm::vec4{1.1f, 1.1f, 1.1f, 1.0f};
	}

	if (backgroundTexture) {
		if (useNineSlice) {
			renderer.drawNineSlice(*backgroundTexture, rect, nineSliceMargins, 0.0f, bgTint);
		} else {
			renderer.drawTexture(*backgroundTexture, rect, nullptr, 0.0f, 0.0f, bgTint);
		}
	} else {
		renderer.drawQuad(rect, 0.0f, 0.0f, boxColor);
	}

	if (checked) {
		if (checkmarkTexture) {
			SDL_FRect checkRect{
				absPos.x + checkmarkPadding,
				absPos.y + checkmarkPadding,
				size.x - (checkmarkPadding * 2),
				size.y - (checkmarkPadding * 2)

			};

			if (checkmarkScale != 1.0f) {
				float scaledWidth = checkRect.w * checkmarkScale;
				float scaledHeight = checkRect.h * checkmarkScale;
				float offsetX = (checkRect.w - scaledWidth) * 0.5f;
				float offsetY = (checkRect.h - scaledHeight) * 0.5f;

				checkRect.x += offsetX;
				checkRect.y += offsetY;
				checkRect.w = scaledWidth;
				checkRect.h = scaledHeight;
			}

			renderer.drawTexture(*checkmarkTexture, checkRect, nullptr, 0.0f, 0.1f, checkTint);
		} else {
			float checkPadding = size.x * 0.25f;
			SDL_FRect checkRect{
				absPos.x + checkPadding,
				absPos.y + checkPadding,
				size.x - (checkmarkPadding * 2),
				size.y - (checkmarkPadding * 2)

			};

			renderer.drawQuad(checkRect, 0.0f, 0.1f, checkColor);
		}
	}
}

} //namespace Blackthorn::UI