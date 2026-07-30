#pragma once

#include "Graphics/Renderer.h"
#include "ECS/Components/Kinematics.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/ISystem.h"

namespace Blackthorn::ECS::Systems {

class RenderSystem : public ISystem {
	Graphics::Renderer* renderer;

public:
	RenderSystem(Graphics::Renderer* ren) : renderer(ren) {}

	void render(ECS::EntityPool* pool, float alpha) override {
		auto view = pool->view<Components::Sprite, Components::Transform, Components::Kinematics*>();
		view.each([alpha, this](Entity, Components::Sprite& s, Components::Transform& t, Components::Kinematics* k){
			if (!s.texture)
				return;

			glm::vec2 interpolated = k ? glm::mix(k->oldPosition, t.position, alpha)
				: t.position;

			SDL_FRect rect = {
				interpolated.x,
				interpolated.y,
				s.width * t.scale,
				s.height * t.scale
			};

			const bool hasSourceRect = s.sourceRect.w > 0.0f && s.sourceRect.h > 0.0f;
			renderer->drawTexture(*s.texture, rect, hasSourceRect ? &s.sourceRect : nullptr, t.angle, s.zOrder);
		});
	}
};

} // namespace Blackthorn::ECS::Systems