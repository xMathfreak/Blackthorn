#pragma once

#include "Graphics/Renderer.h"
#include "ECS/Components/Kinematics.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/ISystem.h"

namespace Blackthorn::ECS::Systems {

class BLACKTHORN_API RenderSystem : public ISystem {
	Graphics::Renderer& renderer;

public:
	BLACKTHORN_API RenderSystem(Graphics::Renderer& ren) : renderer(ren) {}

	void render(ECS::EntityPool* pool, float alpha) override {
		auto view = pool->view<Components::Sprite, Components::Transform, Components::Kinematics*>();
		view.each([alpha, this](Entity, Components::Sprite& s, Components::Transform& t, Components::Kinematics* k){
			if (!s.texture)
				return;

			glm::vec2 interpolated = k ? glm::mix(k->oldPosition, t.position, alpha)
				: t.position;

			s.dest.x = interpolated.x;
			s.dest.y = interpolated.y;
			s.dest.w = s.src.w * t.scale;
			s.dest.h = s.src.h * t.scale;

			if (s.flipX)
				s.dest.w *= -1;

			if (s.flipY)
				s.dest.y *= -1;

			renderer.drawTexture(*s.texture, s.dest, &s.src, t.angle, s.zOrder);
		});
	}
};

} // namespace Blackthorn::ECS::Systems