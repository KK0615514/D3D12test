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

#include "scene/Game3DScene.h"

int main(int argc, char* argv[]) {
    // 取得模組實例控制代碼    // 建立視窗
    uint32_t windowWidth      = static_cast<uint32_t>(Common::InitialWindowWidth);
    uint32_t windowHeight     = static_cast<uint32_t>(Common::InitialWindowHeight);
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = CreateNativeWindow(windowWidth, windowHeight, hInstance);
    if (!hwnd) {
        std::cerr << "視窗建立失敗！" << std::endl;
        return -1;
    }

    // 核心物件設定
    std::unique_ptr<Timer> m_timer = std::make_unique<Timer>();
    std::unique_ptr<RenderSystem> m_renderSystem = std::make_unique<RenderSystem>();
    std::unique_ptr<InputManager> m_input = std::make_unique<InputManager>();
    m_timer->SetTargetFPS(144);
    m_renderSystem->Init(hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_input.get()));
    GameContext m_ctx{
        *m_timer,
        *m_input
    };

    // 顯示視窗
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    //初始Scene設定 
    std::unique_ptr<IScene> currentScene = std::make_unique<Game3DScene>(m_ctx);
    
    //test  
    currentScene->SetMeshes(
        m_renderSystem->LoadMeshes(currentScene->test)
    );

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

        m_timer->StartFrame();

        currentScene->Update();
        const auto& data = currentScene->GetRenderData();

        //test
        static int a = 0;
        a++;
        if (a >= 144)
        {
            std::cout << "FPS : " << m_timer->GetFPS() << std::endl;
            std::cout << "Instance Counts : " << data.instanceDatas.size() << std::endl;
            a = 0;
        }

        m_renderSystem->Update(data);
        m_renderSystem->Render(data);

        m_input->EndFrame();                       //紀錄上一幀的按鍵
        m_timer->EndFrame();
    }
    return (int)msg.wParam;
}