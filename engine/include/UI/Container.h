#pragma once

#include <memory>
#include <vector>

#include "Core/Export.h"
#include "UI/Widget.h"

namespace Blackthorn::UI {

class BLACKTHORN_API Container : public Widget {
protected:
	std::vector<std::unique_ptr<Widget>> children;
	Widget* getChildAt(const glm::vec2& position);

public:
	Container();
	~Container() override = default;

	void addChild(std::unique_ptr<Widget> child);
	void removeChild(Widget* child);
	void clearChildren();

	const std::vector<std::unique_ptr<Widget>>& getChildren() const { return children; }

	void update(float dt) override;
	void render(Graphics::Renderer& renderer) override;

	bool onMouseMove(const glm::vec2& position) override;
	bool onMouseDown(const glm::vec2& position, Uint8 button) override;
	bool onMouseUp(const glm::vec2& position, Uint8 button) override;
	bool onKeyDown(SDL_Keycode key) override;
	bool onKeyUp(SDL_Keycode key) override;

	virtual void layoutChildren();
};

} // namespace Blackthorn::UI
