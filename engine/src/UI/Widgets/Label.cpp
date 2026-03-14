#include "UI/Widgets/Label.h"
#include "Fonts/BitmapFont.h"
#include "Fonts/Font.h"
#include "UI/Container.h"
#include "UI/UIManager.h"

namespace Blackthorn::UI {

Label::Label(const std::string& t, Mode m)
	: text(t), renderMode(m)
{
	widthMode = SizeMode::Content;
	heightMode = SizeMode::Content;
}

void Label::render(Graphics::Renderer& renderer) {
	if (!visible || !font || text.empty())
		return;

	updateLayout();

	const float scale = UIManager::getEffectiveScale();
	glm::vec2 absPos = getAbsolutePosition();

	glm::vec2 contentPos;
	contentPos.x = absPos.x + padding.left * scale;
	contentPos.y = absPos.y + padding.top * scale;

	glm::vec2 contentSize;
	contentSize.x = (size.x - padding.left - padding.right) * scale;
	contentSize.y = (size.y - padding.top - padding.bottom) * scale;

	glm::vec2 renderPos = contentPos;

	switch (textAlignment) {
		case Text::Alignment::Left:
			break;
		case Text::Alignment::Center:
			renderPos.x += contentSize.x * 0.5f;
			break;
		case Text::Alignment::Right:
			renderPos.x += contentSize.x;
			break;
	}

	if (dynamic_cast<Fonts::BitmapFont*>(font)) {
		renderPos.x = std::floor(renderPos.x + 0.5f);
		renderPos.y = std::floor(renderPos.y + 0.5f);
	}

	const float effectiveScale = textScale * scale;

	if (renderMode == Mode::Dynamic) {
		font->draw(text, renderPos, effectiveScale, zDepth, 0.0f, textColor, textAlignment);
	} else {
		font->drawCached(text, renderPos, effectiveScale, zDepth, 0.0f, textColor, textAlignment);
	}

	renderDirty = false;
}

glm::vec2 Label::getMinimumSize() const {
	if (!font || text.empty())
		return {padding.left + padding.right, padding.top + padding.bottom};

	if (textSizeDirty)
		updateTextSize();

	return {
		cachedTextSize.x + padding.left + padding.right,
		cachedTextSize.y + padding.top + padding.bottom
	};
}

void Label::updateLayout() {
	if (!layoutDirty)
		return;

	if (widthMode == SizeMode::Content || heightMode == SizeMode::Content) {
		glm::vec2 minSize = getMinimumSize();

		if (widthMode == SizeMode::Content) {
			size.x = minSize.x;
			designWidth = minSize.x;
		}

		if (heightMode == SizeMode::Content) {
			size.y = minSize.y;
			designHeight = minSize.y;
		}
	}

	layoutDirty = false;
	markTransformDirty();
}

void Label::updateTextSize() const {
	if (!font || text.empty()) {
		cachedTextSize = {0, 0};
		textSizeDirty = false;
		return;
	}

	float maxWidth = parent ? parent->getSize().x : 0.0f;

	auto measurement = font->measure(text, textScale, maxWidth);
	cachedTextSize = {measurement.width, measurement.height};
	textSizeDirty = false;
}

void Label::setText(const std::string& t) {
	if (text == t)
		return;

	text = t;
	textSizeDirty = true;

	markLayoutDirty();
	markRenderDirty();
}

void Label::setTextColor(const Math::Color& color) {
	if (textColor == color)
		return;

	textColor = color;
	markRenderDirty();
}

void Label::setFont(Fonts::Font* f) {
	if (font == f)
		return;

	font = f;
	textSizeDirty = true;

	markLayoutDirty();
	markRenderDirty();
}

void Label::setScale(float scale) {
	if (textScale == scale)
		return;

	textScale = scale;
	textSizeDirty = true;

	markLayoutDirty();
	markRenderDirty();
}

void Label::setTextAlignment(Text::Alignment align) {
	if (textAlignment == align)
		return;

	textAlignment = align;
	markRenderDirty();
}

void Label::setMode(Mode mode) {
	if (renderMode == mode)
		return;

	renderMode = mode;
	markRenderDirty();
}

void Label::setZDepth(float z) {
	if (zDepth == z)
		return;

	zDepth = z;
	markRenderDirty();
}

void Label::markLayoutDirty() {
	Widget::markLayoutDirty();
	textSizeDirty = true;
}

} // namespace Blackthorn::UI