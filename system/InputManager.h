#pragma once
#include <cstring>

class InputManager{
public:
	InputManager() = default;
	~InputManager() = default;

	InputManager(const InputManager&) = delete;
	InputManager(InputManager&&) = delete;
	InputManager& operator=(const InputManager&) = delete;
	InputManager& operator=(InputManager&&) = delete;

	void SetKeyState(int vKey, bool state) {
		if (vKey >= 0 && vKey < 256) {
			currentKeys[vKey] = state;
		}
	}

	void SetMousePos(int x, int y){
		mouseX = x;
		mouseY = y;
	}

	int GetMouseX() {
		return mouseX;
	}

	int GetMouseY() {
		return mouseY;
	}

	bool IsKeyHeld(int vKey) {
		return currentKeys[vKey];
	}

	bool IsKeyDown(int vKey) {
		return currentKeys[vKey] && !PreviousKeys[vKey];
	}

	bool IsKeyUp(int vKey) {
		return !currentKeys[vKey] && PreviousKeys[vKey];
	}

	void EndFrame() {
		std::memcpy(PreviousKeys, currentKeys, sizeof(currentKeys));
	}

private:
	bool currentKeys[256]{ false };
	bool PreviousKeys[256]{ false };

	int mouseX;
	int mouseY;
};