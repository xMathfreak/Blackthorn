#pragma once

#include <memory>
#include <vector>

#include "Core/Export.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

class BLACKTHORN_API Container : public Widget {
public:
	enum class LayoutType : U8 {
		None,
		Vertical,
		Horizontal,
		Grid
	};

	enum class SizingMode : U8 {
		Fixed,
		FitContent,
		FillParent
	};

protected:
	std::vector<std::unique_ptr<Widget>> widgets;

	LayoutType layoutType = LayoutType::None;
	SizingMode sizingMode = SizingMode::Fixed;

	float spacing = 0.0f;
	U8 gridColumns = 1;

	void applyNoLayout();
	void applyVerticalLayout();
	void applyHorizontalLayout();
	void applyGridLayout();

	glm::vec2 getWidgetSize(Widget* widget, const glm::vec2& availableSpace) const;

public:
	Container();
	~Container() override = default;

	Container(const Container&) = delete;
	Container& operator=(const Container&) = delete;

	Container(Container&&) noexcept = default;
	Container& operator=(Container&&) noexcept = default;

	void update(float dt) override;
	void render(Graphics::Renderer& renderer) override;

	bool onMouseMove(const glm::vec2& pos) override;
	bool onMouseDown(const glm::vec2& pos, U8 button) override;
	bool onMouseUp(const glm::vec2& pos, U8 button) override;

	void addWidget(std::unique_ptr<Widget> widget);
	void removeWidget(Widget* widget);
	void clearWidgets();

	const std::vector<std::unique_ptr<Widget>>& getWidgets() const { return widgets; }
	size_t getWidgetCount() const { return widgets.size(); }

	void setLayoutType(LayoutType type);
	LayoutType getLayoutType() const { return layoutType; }

	void setSpacing(float space);
	float getSpacing() const { return spacing; }

	void setGridColumns(U8 cols);
	U8 getGridColumns() const { return gridColumns; }

	void setSizingMode(SizingMode mode);
	SizingMode getSizingMode() const { return sizingMode; }

	glm::vec2 calculateContentSize() const;
	glm::vec2 getMinimumSize() const override;

	void markTransformDirty() override;
	void markLayoutDirty() override;
	void markRenderDirty() override;

	void updateLayout() override;
};

} // namespace Blackthorn::UI