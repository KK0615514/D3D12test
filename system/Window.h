#pragma once
#include <windows.h>
#include <windowsx.h>
#include <iostream>

#include "InputManager.h"

LRESULT CALLBACK WindowProc(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);

HWND CreateNativeWindow(int width, int height, HINSTANCE hInstance);