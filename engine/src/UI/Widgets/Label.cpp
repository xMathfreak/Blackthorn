#include "UI/Widgets/Label.h"
#include "Fonts/Font.h"

namespace Blackthorn::UI {

Label::Label(const std::string& t, Mode m)
	: renderMode(m)
{
	setText(t);
}

void Label::render(Graphics::Renderer& renderer) {
	if (!font || text.empty())
		return;

	glm::vec2 absPos = getAbsolutePosition();

	if (renderMode == Mode::Dynamic) {
		font->draw(text, absPos, textScale, 0.0f, 0.0f, textColor);
	} else {
		font->drawCached(text, absPos, textScale, 0.0f, 0.0f, textColor);
	}
}

glm::vec2 Label::getMinimumSize() const {
	if (!font)
		return glm::vec2{0};

	auto m = font->measure(text, textScale, 0.0f);
	return {m.width, m.height};
}

void Label::setText(const std::string& t) {
	if (t.empty())
		return;

	text = t;

	if (widthMode == SizeMode::Content || heightMode == SizeMode::Content) {
		glm::vec2 minSize = getMinimumSize();

		if (widthMode == SizeMode::Content)
			size.x = minSize.x;

		if (heightMode == SizeMode::Content)
			size.y = minSize.y;
	}
}

} // namespace Blackthorn::UI