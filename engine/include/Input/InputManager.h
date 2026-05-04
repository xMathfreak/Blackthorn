#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Input {

enum class ButtonState {
	Up,
	Pressed,
	Down,
	Released
};

class BLACKTHORN_API InputManager {
private:
	std::unordered_map<SDL_Keycode, ButtonState> keyStates;

	glm::vec2 mousePosition{0, 0};
	glm::vec2 mouseDelta{0, 0};
	glm::vec2 mouseWheel{0, 0};
	std::array<ButtonState, 5> mouseButtons{};

	struct ActionBinding {
		SDL_Keycode primary;
		SDL_Keycode alternative;
	};

	std::unordered_map<std::string, ActionBinding> actions;

	bool textInputEnabled = false;
	std::string textInput;

public:
	InputManager();

	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	/** @brief Returns the human readable name of an SDL_Keycode. */
	static std::string keyName(SDL_Keycode key);

	/** @brief Resolves a human readable key name to an SDL_Keycode. */
	static SDL_Keycode keyFromName(const std::string& name);

	void handleEvent(const SDL_Event& event);
	void update(float dt);

	bool isKeyDown(SDL_Keycode key) const {
		auto it = keyStates.find(key);
		if (it == keyStates.end())
			return false;

		return it->second == ButtonState::Down
			|| it->second == ButtonState::Pressed;
	}

	bool isKeyPressed(SDL_Keycode key) const {
		auto it = keyStates.find(key);
		return it != keyStates.end() && it->second == ButtonState::Pressed;
	}

	bool isKeyReleased(SDL_Keycode key) const {
		auto it = keyStates.find(key);
		return it != keyStates.end() && it->second == ButtonState::Released;
	}

	bool isKeyUp(SDL_Keycode key) const {
		auto it = keyStates.find(key);
		return it == keyStates.end() || it->second == ButtonState::Up;
	}

	glm::vec2 getMousePosition() const {
		return mousePosition;
	}

	glm::vec2 getMouseWorldPosition(const glm::vec2& cameraPos, float cameraZoom, const glm::vec2& screenSize) const {
		glm::vec2 offset = mousePosition - screenSize * 0.5f;
		return cameraPos + offset / cameraZoom;
	}

	glm::vec2 getMouseDelta() const {
		return mouseDelta;
	}

	glm::vec2 getMouseWheel() const {
		return mouseWheel;
	}

	bool isMouseButtonDown(U8 button) const {
		if (button >= mouseButtons.size())
			return false;

		return mouseButtons[button] == ButtonState::Down
			|| mouseButtons[button] == ButtonState::Pressed;
	}

	bool isMouseButtonPressed(U8 button) const {
		if (button >= mouseButtons.size())
			return false;

		return mouseButtons[button] == ButtonState::Pressed;
	}

	bool isMouseButtonReleased(U8 button) const {
		if (button >= mouseButtons.size())
			return false;

		return mouseButtons[button] == ButtonState::Released;
	}

	void registerAction(const std::string& action, SDL_Keycode key, SDL_Keycode altKey = SDLK_UNKNOWN) {
		actions[action] = {key, altKey};
	}

	void registerAction(const std::string& action, const std::string& primaryKey, const std::string& altKey = "");

	bool isActionDown(const std::string& action) const {
		auto it = actions.find(action);
		if (it != actions.end()) {
			if (isKeyDown(it->second.primary))
				return true;

			if (it->second.alternative != SDLK_UNKNOWN && isKeyDown(it->second.alternative))
				return true;
		}

		return false;
	}

	bool isActionPressed(const std::string& action) const {
		auto it = actions.find(action);
		if (it != actions.end()) {
			if (isKeyPressed(it->second.primary))
				return true;

			if (it->second.alternative != SDLK_UNKNOWN && isKeyPressed(it->second.alternative))
				return true;
		}

		return false;
	}

	void setTextInputEnabled(SDL_Window* window, bool enabled) {
		if (enabled) {
			SDL_StartTextInput(window);
		} else {
			SDL_StopTextInput(window);
		}
	}

	const std::string& getTextInput() const {
		return textInput;
	}

	void clearTextInput() {
		textInput.clear();
	}

	void saveBindingsToSettings() const;

	void loadBindingsFromSettings();

private:
	void updateButtonState(ButtonState& state) {
		if (state == ButtonState::Pressed) {
			state = ButtonState::Down;
		} else if (state == ButtonState::Released) {
			state = ButtonState::Up;
		}
	}
};

} // namespace Blackthorn::Input