#include "Window.h"


//視窗事件回呼函式 (Window Procedure)
LRESULT CALLBACK WindowProc(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:            //右上關閉
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:               //更動視窗大小
        return 0;

    case WM_KEYDOWN:            //鍵盤按下啟動 鬆開關閉
        InputManager::SetKeyState(wParam,true);
        return 0;
    case WM_KEYUP:
        InputManager::SetKeyState(wParam, false);
        return 0;

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        InputManager::SetMousePos(x,y);
        return 0;
    }
    case WM_LBUTTONDOWN:        //滑鼠按下啟動 鬆開關閉
        InputManager::SetKeyState(VK_LBUTTON, true);
        return 0;
    case WM_LBUTTONUP:
        InputManager::SetKeyState(VK_LBUTTON, false);
        return 0;
    case WM_RBUTTONDOWN:        
        InputManager::SetKeyState(VK_RBUTTON, true);
        return 0;
    case WM_RBUTTONUP:
        InputManager::SetKeyState(VK_RBUTTON, false);
        return 0;
    }



    return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND CreateNativeWindow(int width, int height, HINSTANCE hInstance) {
    // 註冊視窗類別
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"D3D12EngineWindowClass";
    RegisterClassExW(&wc);

    // 計算實際含邊框大小
    RECT windowRect = { 0, 0, width, height };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);


    const wchar_t* windowTitle = L"うんこ";


    // 建立視窗
    HWND hwnd = CreateWindowExW(
        0,                                   // 1. 進階視窗擴充樣式 (預設給 0)
        L"D3D12EngineWindowClass",          // 2. 類別名稱 (前面必須有 L)
        windowTitle, // 3. 視窗標題 (前面必須有 L)
        WS_OVERLAPPEDWINDOW,                 // 4. 視窗樣式
        CW_USEDEFAULT, CW_USEDEFAULT,        // 5. 視窗 X, Y 座標
        windowRect.right - windowRect.left,  // 6. 視窗寬度
        windowRect.bottom - windowRect.top,  // 7. 視窗高度
        NULL,                                // 8. 父視窗控制代碼
        NULL,                                // 9. 功能表控制代碼
        hInstance,                           // 10. 應用程式實例控制代碼
        NULL                                 // 11. 建立參數
    );

    return hwnd;
}