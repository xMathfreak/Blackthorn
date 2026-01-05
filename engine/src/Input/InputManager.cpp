#include "Input/InputManager.h"

namespace Blackthorn::Input {

inline InputManager::InputManager() {
	mouseButtons.fill(ButtonState::Up);
}

inline void InputManager::handleEvent(const SDL_Event& event) {
	switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
			if (!event.key.repeat) {
				keyStates[event.key.key] = ButtonState::Pressed;
			} 
			break;

		case SDL_EVENT_KEY_UP:
			keyStates[event.key.key] = ButtonState::Released;
			break;

		case SDL_EVENT_MOUSE_MOTION:
			mousePosition = { event.motion.x, event.motion.y };
			mouseDelta = { event.motion.xrel, event.motion.yrel };
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event.button.button < mouseButtons.size())
				mouseButtons[event.button.button] = ButtonState::Pressed;

			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event.button.button < mouseButtons.size())
				mouseButtons[event.button.button] = ButtonState::Released;
			
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			mouseWheel = { event.wheel.x, event.wheel.y };
			break;

		case SDL_EVENT_TEXT_INPUT:
			if (textInputEnabled)
				textInput += event.text.text;

			break;

		default:
			break;
	}
}

inline void InputManager::update(float dt) {
	for (auto& [key, state] : keyStates)
		updateButtonState(state);


	for (auto& state : mouseButtons)
		updateButtonState(state);

	mouseDelta = { 0, 0 };
	mouseWheel = { 0, 0 };
}

} // namespace Blackthorn::Input