#pragma once

#include "Blackthorn/ECS/World.hpp"
#include "Blackthorn/ECS/Queries.hpp"

#include "Blackthorn/ECS/Components/Kinematics.hpp"
#include "Blackthorn/ECS/Components/Name.hpp"
#include "Blackthorn/ECS/Components/Persistent.hpp"
#include "Blackthorn/ECS/Components/Tag.hpp"
#include "Blackthorn/ECS/Components/Transform.hpp"

#include "Blackthorn/ECS/Systems/KinematicsSystem.hpp"

#ifdef BLACKTHORN_CLIENT
	#include "Blackthorn/ECS/Components/Sprite.hpp"
	#include "Blackthorn/ECS/Components/SpriteAnimation.hpp"
	#include "Blackthorn/ECS/Systems/AnimationSystem.hpp"
	#include "Blackthorn/ECS/Systems/RenderSystem.hpp"
#endif