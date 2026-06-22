#pragma once

namespace Blackthorn::Audio { class AudioManager; }

namespace Blackthorn::Editor {

class AudioPreviewContext {
public:
	static void setManager(Audio::AudioManager* mgr) { activeManager = mgr; }
	static Audio::AudioManager* manager() { return activeManager; }

private:
	static inline Audio::AudioManager* activeManager = nullptr;
};

} // namespace Blackthorn::Editor