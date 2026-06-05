#pragma once

#include <glm/glm.hpp>

#include "Audio/AudioCategory.h"
#include "Audio/AudioHandle.h"
#include "Audio/Resources/AudioClip.h"

namespace Blackthorn::Audio {

struct VoiceSnapshot {
	AudioHandle originalHandle;
	const AudioClip* clip = nullptr;

	float volume = 1.0f;
	float pitch = 1.0f;
	float playbackTime = 0.0f;
	float duration = 0.0f;

	glm::vec3 position {0.0f};
	float minDistance = 1.0f;
	float maxDistance = 50.0f;

	AudioCategory category = AudioCategory::SFX;
	int priority = 0;

	bool loop = false;
	bool spatial = false;
	bool stream = false;
};

} // Blackthorn::Audio