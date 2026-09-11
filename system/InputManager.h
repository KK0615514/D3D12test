#pragma once
#include <cstring>

class InputManager{
public:
	InputManager(const InputManager&) = delete;
	InputManager(InputManager&&) = delete;
	InputManager& operator=(const InputManager&) = delete;
	InputManager& operator=(InputManager&&) = delete;


	static void SetKeyState(int vKey, bool state) {
		if (vKey >= 0 && vKey < 256) {
			Get().currentKeys[vKey] = state;
		}
	}

	static void SetMousePos(int x, int y){
		Get().mouseX = x;
		Get().mouseY = y;
	}

	static int GetMouseX() {
		return Get().mouseX;
	}

	static int GetMouseY() {
		return Get().mouseY;
	}

	static bool IsKeyHeld(int vKey) {
		return Get().currentKeys[vKey];
	}

	static bool IsKeyDown(int vKey) {
		return Get().currentKeys[vKey] && !Get().PreviousKeys[vKey];
	}

	static bool IsKeyUp(int vKey) {
		return !Get().currentKeys[vKey] && Get().PreviousKeys[vKey];
	}

	static void EndFrame() {
		std::memcpy(Get().PreviousKeys, Get().currentKeys, sizeof(Get().currentKeys));
	}

private:
	bool currentKeys[256]{ false };
	bool PreviousKeys[256]{ false };

	int mouseX;
	int mouseY;

	static InputManager& Get() {
		static InputManager instance; 
		return instance;
	}

	InputManager() = default;
	~InputManager() = default;
};