#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>    //前面兩個ifndef 再瘦身windows
#include <tchar.h>
#include <iostream>

#include "system/Timer.h"
#include "system/RenderSystem.h"
#include "system/InputManager.h"
#include "system/Window.h"

#include "scene/MenuScene.h"
#include "scene/GameScene.h"
#include "scene/ResultScene.h"

//複製貼上
#include "gameobject/Audio.h"

int main(int argc, char* argv[]) {
    // 取得模組實例控制代碼    // 建立視窗
    int windowWidth = 1280;
    int windowHeight = 720;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = CreateNativeWindow(windowWidth, windowHeight, hInstance);
    if (!hwnd) {
        std::cerr << "視窗建立失敗！" << std::endl;
        return -1;
    }

    // 核心物件設定
    Timer::GetInstance().SetTargetFPS(80);
    RenderSystem::GetInstance().Init(hwnd);

    // 顯示視窗
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    //初始Scene設定 複製的audio 
    std::unique_ptr<IScene> currentScene = std::make_unique<GameScene>();
    if (!SimpleAudio::Get().Initialize()) {
        MessageBoxW(NULL, L"XAudio2 初始化失敗！", L"錯誤", MB_OK);
        return -1;
    }

    // ─── 主迴圈 ───
    MSG msg = {};
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        Timer::GetInstance().StartFrame();

        //暫時用
        std::unique_ptr<IScene> nextScene = currentScene->ChangeScene();
        //if (nextScene != nullptr) currentScene = std::move(nextScene);
        
        static float abc = 0.0f;
        abc += Timer::GetInstance().GetDeltaTime();
        if (abc >= 1.0f) {
            std::cout << Timer::GetInstance().GetFPS() << std::endl;
            std::cout << currentScene->m_enemys.size() << std::endl;
            abc -= 1.0f;
        }

        currentScene->Update();

        RenderSystem::GetInstance().Update(*currentScene);
        RenderSystem::GetInstance().Render();

        InputManager::EndFrame();                       //紀錄上一幀的按鍵
        Timer::GetInstance().EndFrame();
    }
    return (int)msg.wParam;
}