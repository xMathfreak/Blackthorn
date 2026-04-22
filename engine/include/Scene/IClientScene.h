#pragma once

#include "Core/Export.h"
#include "Scene/IScene.h"
#include "Scene/ISceneContext.h"
#include "UI/UIManager.h"

namespace Blackthorn::Scene {

class BLACKTHORN_API IClientScene : public IScene {
protected:
	std::unique_ptr<UI::UIManager> uiManager;
	auto& getContext() { return static_cast<ISceneContext&>(context); }

public:
	explicit IClientScene(ISimContext& ctx)
		: IScene(ctx)
	{}

	~IClientScene() override = default;

	void init() override {
		IScene::init();
		uiManager = std::make_unique<UI::UIManager>();
	}

	void update(float dt) override {
		IScene::update(dt);

		if (uiManager) {
			uiManager->update(dt);
			uiManager->handleInput(getContext().getInputManager());
		}
	}

	virtual void render(float alpha) {
		if (world)
			world->render(alpha);

		if (uiManager)
			uiManager->render(getContext().getRenderer());
	}

	UI::UIManager* getUIManager() { return uiManager.get(); }
	const UI::UIManager* getUIManager() const { return uiManager.get(); }
};

} // namespace Blackthorn::Scene