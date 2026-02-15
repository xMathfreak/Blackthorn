#include "UI/Layout.h"

#include "UI/Container.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

glm::vec2 Layout::getWidgetSize(Widget* widget, const glm::vec2& availableSpace) {
	glm::vec2 size = widget->getSize();
	auto sizeModes = widget->getSizeModes();
	auto dimensions = widget->getDimensions();

	if (sizeModes.widthMode == SizeMode::Content) {
		size.x = widget->getMinimumSize().x;
	} else if (sizeModes.widthMode == SizeMode::Percent) {
		size.x = availableSpace.x * (dimensions.width / 100.0f);
	}

	if (sizeModes.heightMode == SizeMode::Content) {
		size.y = widget->getMinimumSize().y;
	} else if (sizeModes.heightMode == SizeMode::Percent) {
		size.y = availableSpace.y * (dimensions.height / 100.0f);
	}

	const auto& margin = widget->getMargin();
	size.x += margin.left + margin.right;
	size.y = margin.top + margin.bottom;

	return size;
}

void Layout::applyAlignment(Widget* widget, const glm::vec2& position, const glm::vec2& allocatedSize, const Alignment& alignment) {
	const auto& margin = widget->getMargin();
	glm::vec2 widgetSize = widget->getSize();
	glm::vec2 finalPosition = position;
	glm::vec2 finalSize = widgetSize;

	switch (alignment.horizontal) {
		case Alignment::Horizontal::Left:
			finalPosition += margin.left;
			break;

		case Alignment::Horizontal::Center:
			finalPosition.x += (allocatedSize.x - widgetSize.x - margin.left - margin.right) * 0.5f + margin.left;
			break;

		case Alignment::Horizontal::Right:
			finalPosition.x += allocatedSize.x - widgetSize.x - margin.right;
			break;

		case Alignment::Horizontal::Stretch:
			finalPosition.x += margin.left;
			finalSize.x = allocatedSize.x - margin.left - margin.right;
		break;
	}

	switch (alignment.vertical) {
		case Alignment::Vertical::Top:
			finalPosition.y += margin.top;
			break;

		case Alignment::Vertical::Center:
			finalPosition.y += (allocatedSize.y - widgetSize.y - margin.top - margin.bottom) * 0.5f + margin.top;
			break;

		case Alignment::Vertical::Bottom:
			finalPosition.y += allocatedSize.y - widgetSize.y - margin.bottom;
			break;

		case Alignment::Vertical::Stretch:
			finalPosition.y += margin.top;
			finalSize.y = allocatedSize.y - margin.top - margin.bottom;
			break;
	}

	widget->setPosition(finalPosition);
	widget->setSize(finalSize);
}

void VerticalLayout::apply(Container* container) {
	if (!container)
		return;

	const auto& children = container->getChildren();
	if (children.empty())
		return;

	const auto& padding = container->getPadding();
	glm::vec2 availableSpace = container->getSize();

	availableSpace.x -= padding.left + padding.right;
	availableSpace.y -= padding.top + padding.bottom;

	float currentY = padding.top;

	for (auto& child : children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), availableSpace);
		glm::vec2 allocatedSize{availableSpace.x, childSize.y};

		applyAlignment(child.get(), glm::vec2{padding.left, currentY}, allocatedSize, child->getAlignment());

		currentY += childSize.y + spacing;
	}
}

glm::vec2 VerticalLayout::calculateMinimumSize(Container* container) {
	if (!container)
		return glm::vec2{0};

	const auto& children = container->getChildren();
	const auto& padding = container->getPadding();

	float maxWidth = 0.0f;
	float totalHeight = padding.top + padding.bottom;
	int visibleCount = 0;

	for (auto& child: children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), glm::vec2{1000000});
		maxWidth = std::max(maxWidth, childSize.x);
		totalHeight += childSize.y;
		visibleCount++;
	}

	if (visibleCount > 1)
		totalHeight += spacing * (visibleCount - 1);

	return glm::vec2{maxWidth + padding.left + padding.right, totalHeight};
}

void HorizontalLayout::apply(Container* container) {
	if (!container)
		return;

	const auto& children = container->getChildren();
	if (children.empty())
		return;

	const auto& padding = container->getPadding();
	glm::vec2 availableSpace = container->getSize();

	availableSpace.x -= padding.left + padding.right;
	availableSpace.y -= padding.top + padding.bottom;

	float currentX = padding.left;

	for (auto& child : children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), availableSpace);
		glm::vec2 allocatedSize{childSize.x, availableSpace.y};

		applyAlignment(child.get(), glm::vec2{currentX, padding.top}, allocatedSize, child->getAlignment());

		currentX += childSize.x + spacing;
	}
}

glm::vec2 HorizontalLayout::calculateMinimumSize(Container* container) {
	if (!container)
		return glm::vec2{0};

	const auto& children = container->getChildren();
	const auto& padding = container->getPadding();

	float totalWidth = padding.left + padding.right;
	float maxHeight = 0.0f;
	int visibleCount = 0;

	for (auto& child : children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), glm::vec2{1000000});
		totalWidth += childSize.x;
		maxHeight = std::max(maxHeight, childSize.y);
		visibleCount++;
	}

	if (visibleCount > 1)
		totalWidth += spacing * (visibleCount - 1);

	return glm::vec2{totalWidth, maxHeight + padding.top + padding.bottom};
}

GridLayout::GridLayout(Uint8 cols)
	: columns(cols)
{}

void GridLayout::apply(Container* container) {
	if (!container || columns <= 0)
		return;

	const auto& children = container->getChildren();
	if (children.empty())
		return;

	const auto& padding = container->getPadding();
	glm::vec2 availableSpace = container->getSize();

	availableSpace.x -= padding.left + padding.right;
	availableSpace.y -= padding.top + padding.bottom;

	float cellWidth = (availableSpace.x - spacing * (columns - 1)) / columns;

	int col = 0;
	int row = 0;

	for (auto& child : children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), glm::vec2{cellWidth, 1000000});

		float x = padding.left + col * (cellWidth + spacing);
		float y = padding.top + row * (childSize.y + spacing);

		glm::vec2 allocatedSize{cellWidth, childSize.y};

		applyAlignment(child.get(), glm::vec2{x, y}, allocatedSize, child->getAlignment());

		col++;

		if (col >= columns) {
			col = 0;
			row++;
		}
	}
}

glm::vec2 GridLayout::calculateMinimumSize(Container* container) {
	if (!container || columns <= 0)
		return glm::vec2{0};

	const auto& children = container->getChildren();
	const auto& padding = container->getPadding();

	if (children.empty())
		return glm::vec2{padding.left + padding.right, padding.top + padding.bottom};

	std::vector<float> columnWidths(columns, 0.0f);
	std::vector<float> rowHeights;

	int col = 0;
	int row = 0;

	for (auto& child : children) {
		if (!child->isVisible())
			continue;

		glm::vec2 childSize = getWidgetSize(child.get(), glm::vec2{1000000});
		columnWidths[col] = std::max(columnWidths[col], childSize.x);

		if (row >= static_cast<int>(rowHeights.size())) {
			rowHeights.push_back(childSize.y);
		} else {
			rowHeights[row] = std::max(rowHeights[row], childSize.y);
		}

		col++;

		if (col >= columns) {
			col = 0;
			row++;
		}
	}

	float totalWidth = padding.left + padding.right;
	for (float w : columnWidths)
		totalWidth += w;

	float totalHeight = padding.top + padding.bottom;
	for (float h : rowHeights)
		totalHeight += h;

	totalHeight += spacing * (rowHeights.size() - 1);

	return glm::vec2{totalWidth, totalHeight};
}

} // namespace Blackthorn::UI