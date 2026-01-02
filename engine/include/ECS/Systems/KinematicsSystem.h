#pragma once

#include "ECS/Components/Kinematics.h"
#include "ECS/Components/Transform.h"
#include "ECS/ISystem.h"

namespace Blackthorn::ECS::Systems {

class BLACKTHORN_API KinematicsSystem : public ISystem {
public:
	void fixedUpdate(EntityPool* pool, float dt) override {
		auto view = pool->view<Components::Kinematics, Components::Transform>();
		float dt2 = dt * dt;
		view.each([dt2](Entity, Components::Kinematics& k, Components::Transform& t) {
			glm::vec2 temp = t.position;
			t.position = t.position * 2.0f - k.oldPosition + k.acceleration * dt2;
			k.oldPosition = temp;
			k.acceleration *= 0;
		});
	}
};

} // namespace Blackthorn::ECS::Systems