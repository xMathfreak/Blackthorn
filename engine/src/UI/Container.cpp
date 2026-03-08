#include "UI/Container.h"
#include "Graphics/Renderer.h"
#include "UI/UIManager.h"
#include <algorithm>

namespace Blackthorn::UI {

Container::Container() {
	widgets.reserve(8);
}

void Container::render(Graphics::Renderer& renderer) {
	if (!visible)
		return;

	updateLayout();

	Widget::render(renderer);

	for (auto& widget : widgets) {
		if (widget && widget->isVisible())
			widget->render(renderer);
	}
}

void Container::update(float dt) {
	for (auto& widget : widgets) {
		if (widget && widget->isVisible())
			widget->update(dt);
	}
}

bool Container::onMouseMove(const glm::vec2& pos) {
	if (!visible)
		return false;

	for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
		if (*it && (*it)->isVisible())
			(*it)->onMouseMove(pos);
	}

	return Widget::onMouseMove(pos);
}

bool Container::onMouseDown(const glm::vec2& pos, Uint8 button) {
	if (!visible || !isEnabled())
		return false;

	for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
		if (*it && (*it)->isVisible() && (*it)->onMouseDown(pos, button))
			return true;
	}

	return Widget::onMouseDown(pos, button);
}

bool Container::onMouseUp(const glm::vec2& pos, Uint8 button) {
	if (!visible)
		return false;

	for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
		if (*it && (*it)->isVisible() && (*it)->onMouseUp(pos, button))
			return true;
	}

	return Widget::onMouseUp(pos, button);
}

void Container::addWidget(std::unique_ptr<Widget> widget) {
	if (!widget)
		return;

	widget->setParent(this);
	widgets.push_back(std::move(widget));
	markLayoutDirty();
}

void Container::removeWidget(Widget* widget) {
	if (!widget)
		return;

	auto it = std::find_if(widgets.begin(), widgets.end(),
		[widget](const std::unique_ptr<Widget>& ptr) {
			return ptr.get() == widget;
		}
	);

	if (it != widgets.end()) {
		widgets.erase(it);
		markLayoutDirty();
	}
}

void Container::clearWidgets() {
	widgets.clear();
	markLayoutDirty();
}

void Container::setLayoutType(LayoutType type) {
	if (layoutType == type)
		return;

	layoutType = type;
	markLayoutDirty();
}

void Container::setSpacing(float space) {
	if (spacing == space)
		return;

	spacing = space;
	markLayoutDirty();
}

void Container::setGridColumns(Uint8 cols) {
	if (cols == 0) cols = 1;

	if (gridColumns == cols)
		return;

	gridColumns = cols;
	if (layoutType == LayoutType::Grid)
		markLayoutDirty();
}

void Container::setSizingMode(SizingMode mode) {
	if (sizingMode == mode)
		return;

	sizingMode = mode;
	markLayoutDirty();
}

glm::vec2 Container::calculateContentSize() const {
	if (widgets.empty())
		return {padding.left + padding.right, padding.top + padding.bottom};

	glm::vec2 contentSize{0};

	switch (layoutType) {
		case LayoutType::None: {
			float maxX = 0, maxY = 0;
			for (const auto& widget : widgets) {
				if (!widget || !widget->isVisible())
					continue;

				glm::vec2 widgetPos = widget->getPosition();
				glm::vec2 widgetSize = widget->getSize();
				const auto& m = widget->getMargin();

				maxX = std::max(maxX, widgetPos.x + widgetSize.x + m.right);
				maxY = std::max(maxY, widgetPos.y + widgetSize.y + m.bottom);
			}
			contentSize = {maxX, maxY};
			break;
		}

		case LayoutType::Vertical: {
			float maxWidth = 0;
			float totalHeight = 0;
			int visibleCount = 0;

			for (const auto& widget : widgets) {
				if (!widget || !widget->isVisible())
					continue;

				glm::vec2 ws = getWidgetSize(widget.get(), {1e6f, 1e6f});
				maxWidth = std::max(maxWidth, ws.x);
				totalHeight += ws.y;
				++visibleCount;
			}

			if (visibleCount > 1)
				totalHeight += spacing * (visibleCount - 1);

			contentSize = {maxWidth, totalHeight};
			break;
		}

		case LayoutType::Horizontal: {
			float totalWidth = 0;
			float maxHeight = 0;
			int visibleCount = 0;

			for (const auto& widget : widgets) {
				if (!widget || !widget->isVisible())
					continue;

				glm::vec2 ws = getWidgetSize(widget.get(), {1e6f, 1e6f});
				totalWidth += ws.x;
				maxHeight = std::max(maxHeight, ws.y);
				++visibleCount;
			}

			if (visibleCount > 1)
				totalWidth += spacing * (visibleCount - 1);

			contentSize = {totalWidth, maxHeight};
			break;
		}

		case LayoutType::Grid: {
			if (gridColumns == 0)
				break;

			std::vector<float> columnWidths(gridColumns, 0.0f);
			std::vector<float> rowHeights;

			int col = 0, row = 0;
			for (const auto& widget : widgets) {
				if (!widget || !widget->isVisible())
					continue;

				glm::vec2 ws = getWidgetSize(widget.get(), {1e6f, 1e6f});
				columnWidths[col] = std::max(columnWidths[col], ws.x);

				if (row >= static_cast<int>(rowHeights.size()))
					rowHeights.push_back(ws.y);
				else
					rowHeights[row] = std::max(rowHeights[row], ws.y);

				if (++col >= gridColumns) { col = 0; ++row; }
			}

			float totalWidth = 0;
			for (float w : columnWidths) totalWidth += w;
			if (gridColumns > 1)
				totalWidth += spacing * (gridColumns - 1);

			float totalHeight = 0;
			for (float h : rowHeights) totalHeight += h;
			if (rowHeights.size() > 1)
				totalHeight += spacing * (static_cast<float>(rowHeights.size()) - 1);

			contentSize = {totalWidth, totalHeight};
			break;
		}
	}

	contentSize.x += padding.left + padding.right;
	contentSize.y += padding.top + padding.bottom;

	return contentSize;
}

glm::vec2 Container::getMinimumSize() const {
	return calculateContentSize();
}

void Container::updateLayout() {
	if (!layoutDirty)
		return;

	switch (sizingMode) {
		case SizingMode::Fixed:
			break;

		case SizingMode::FitContent:
			size = calculateContentSize();
			break;

		case SizingMode::FillParent:
			if (parent) {
				glm::vec2 parentSize = parent->getSize();
				size.x = parentSize.x - parent->getPadding().left - parent->getPadding().right;
				size.y = parentSize.y - parent->getPadding().top - parent->getPadding().bottom;
			} else {
				size = UIManager::getScreenDimensions() / UIManager::getGlobalUIScale();
			}
			break;
	}

	switch (layoutType) {
		case LayoutType::None:
			applyNoLayout();
			break;
		case LayoutType::Vertical:
			applyVerticalLayout();
			break;
		case LayoutType::Horizontal:
			applyHorizontalLayout();
			break;
		case LayoutType::Grid:
			applyGridLayout();
			break;
	}

	layoutDirty = false;
	markTransformDirty();
}

void Container::applyNoLayout() {}

void Container::applyVerticalLayout() {
	if (widgets.empty())
		return;

	glm::vec2 available;
	available.x = size.x - padding.left - padding.right;
	available.y = size.y - padding.top - padding.bottom;

	float currentY = padding.top;

	for (auto& widget : widgets) {
		if (!widget || !widget->isVisible())
			continue;

		glm::vec2 ws = getWidgetSize(widget.get(), available);

		float x = padding.left + widget->getMargin().left;
		float y = currentY + widget->getMargin().top;

		widget->setPosition({x, y});

		currentY += ws.y + spacing;
	}
}

void Container::applyHorizontalLayout() {
	if (widgets.empty())
		return;

	glm::vec2 available;
	available.x = size.x - padding.left - padding.right;
	available.y = size.y - padding.top - padding.bottom;

	float currentX = padding.left;

	for (auto& widget : widgets) {
		if (!widget || !widget->isVisible())
			continue;

		glm::vec2 ws = getWidgetSize(widget.get(), available);

		float x = currentX + widget->getMargin().left;
		float y = padding.top + widget->getMargin().top;

		widget->setPosition({x, y});

		currentX += ws.x + spacing;
	}
}

void Container::applyGridLayout() {
	if (widgets.empty() || gridColumns == 0)
		return;

	glm::vec2 available;
	available.x = size.x - padding.left - padding.right;
	available.y = size.y - padding.top - padding.bottom;

	float totalSpacing = spacing * (gridColumns - 1);
	float cellWidth = (available.x - totalSpacing) / gridColumns;

	int col = 0;
	float currentY = padding.top;
	float rowHeight = 0;

	for (auto& widget : widgets) {
		if (!widget || !widget->isVisible())
			continue;

		glm::vec2 ws = getWidgetSize(widget.get(), {cellWidth, 1e6f});
		rowHeight = std::max(rowHeight, ws.y);

		float x = padding.left + col * (cellWidth + spacing) + widget->getMargin().left;
		float y = currentY + widget->getMargin().top;

		widget->setPosition({x, y});

		if (++col >= gridColumns) {
			col = 0;
			currentY += rowHeight + spacing;
			rowHeight = 0;
		}
	}
}

glm::vec2 Container::getWidgetSize(Widget* widget, const glm::vec2& availableSpace) const {
	if (!widget)
		return {0, 0};

	glm::vec2 ws = widget->getSize();

	switch (widget->getWidthMode()) {
		case SizeMode::Content:
			ws.x = widget->getMinimumSize().x;
			break;
		case SizeMode::Percent:
			ws.x = availableSpace.x * widget->getDesignWidth();
			break;
		case SizeMode::Fixed:
			if (ws.x == 0.0f)
				ws.x = widget->getMinimumSize().x;
			break;
	}

	switch (widget->getHeightMode()) {
		case SizeMode::Content:
			ws.y = widget->getMinimumSize().y;
			break;
		case SizeMode::Percent:
			ws.y = availableSpace.y * widget->getDesignHeight();
			break;
		case SizeMode::Fixed:
			if (ws.y == 0.0f)
				ws.y = widget->getMinimumSize().y;
			break;
	}

	const auto& m = widget->getMargin();
	ws.x += m.left + m.right;
	ws.y += m.top + m.bottom;

	return ws;
}

void Container::markTransformDirty() {
	Widget::markTransformDirty();

	for (auto& widget : widgets) {
		if (widget)
			widget->markTransformDirty();
	}
}

void Container::markLayoutDirty() {
	Widget::markLayoutDirty();

	for (auto& widget : widgets) {
		if (widget)
			widget->markLayoutDirty();
	}
}

void Container::markRenderDirty() {
	Widget::markRenderDirty();

	for (auto& widget : widgets) {
		if (widget)
			widget->markRenderDirty();
	}
}

} // namespace Blackthorn::UI