#include "Input/InputManager.h"

#include "Core/Settings.h"
#include "Debug/Logger.h"

namespace Blackthorn::Input {

InputManager::InputManager() {
	mouseButtons.fill(ButtonState::Up);
}

void InputManager::handleEvent(const SDL_Event& event) {
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

void InputManager::update(float dt) {
	for (auto& [key, state] : keyStates)
		updateButtonState(state);


	for (auto& state : mouseButtons)
		updateButtonState(state);

	mouseDelta = { 0, 0 };
	mouseWheel = { 0, 0 };
}

std::string InputManager::keyName(SDL_Keycode key) {
	if (key == SDLK_UNKNOWN)
		return "Unknown";

	const char* name = SDL_GetKeyName(key);
	return name ? name : "Unknown";
}

SDL_Keycode InputManager::keyFromName(const std::string& name) {
	if (name.empty() || name == "Unknown")
		return SDLK_UNKNOWN;

	return SDL_GetKeyFromName(name.c_str());
}

void InputManager::registerAction(const std::string& action, const std::string& primaryKey, const std::string& altKey) {
	this->registerAction(action, keyFromName(primaryKey), keyFromName(altKey));
}

void InputManager::saveBindingsToSettings() const {
	auto& s = Core::Settings::instance();
	for (const auto& [action, binding] : actions) {
		s.set("input", action, keyName(binding.primary));
	}
}

void InputManager::loadBindingsFromSettings() {
	auto& s = Core::Settings::instance();
	for (auto& [action, binding] : actions) {
		if (!s.hasKey("input", action))
			continue;

		const std::string name = s.get<std::string>("input", action);
		const SDL_Keycode key = keyFromName(name);

		if (key == SDLK_UNKNOWN) {
			BT_WARN(
				"InputManager: unknown key name '{}' for action '{}', keeping default.",
				name, action
			);
			continue;
		}

		binding.primary = key;
	}
}

} // namespace Blackthorn::Input