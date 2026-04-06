#include "UI/Widgets/Panel.h"

#include "Graphics/Renderer.h"
#include "Graphics/Texture.h"
#include "UI/UIManager.h"

namespace Blackthorn::UI {

Panel::Panel() = default;

Panel::RenderMode Panel::currentRenderMode() const {
	if (!textureHandle)
		return RenderMode::Solid;

	return (!sliceMargins.isEmpty())
		? RenderMode::NineSlice
		: RenderMode::Texture;
}

SDL_FRect Panel::makeDestRect() const {
	const float scale = UIManager::getEffectiveScale();
	const glm::vec2 pos = getAbsolutePosition();

	return SDL_FRect{
		pos.x,
		pos.y,
		size.x * scale,
		size.y * scale
	};
}

void Panel::render(Graphics::Renderer& renderer) {
	if (!visible)
		return;

	updateLayout();

	const SDL_FRect dest = makeDestRect();

	switch (currentRenderMode()) {
		case RenderMode::Solid:
			renderer.drawQuad(dest, 0.0f, zDepth, color);
			break;

		case RenderMode::Texture:
			renderer.drawTexture(*textureHandle, dest, nullptr, 0.0f, zDepth, color);
			break;

		case RenderMode::NineSlice:
			renderer.drawNineSlice(*textureHandle, dest, sliceMargins, zDepth, color);
			break;
	}

	for (auto& widget : widgets) {
		if (widget && widget->isVisible())
			widget->render(renderer);
	}

	renderDirty = false;
}

void Panel::setTexture(Graphics::Texture* handle) {
	textureHandle = handle;
	markRenderDirty();
}

void Panel::clearTexture() {
	textureHandle = nullptr;
	markRenderDirty();
}

void Panel::setSliceMargins(const Graphics::SliceMargins& sm) {
	if (sliceMargins == sm)
		return;

	sliceMargins = sm;
	markRenderDirty();
}

void Panel::setColor(const Math::Color& c) {
	if (color == c)
		return;

	color = c;
	markRenderDirty();
}

void Panel::setZDepth(float z) {
	if (zDepth == z)
		return;

	zDepth = z;
	markRenderDirty();
}

} // namespace Blackthorn::UI