    #include "Timer.h"

void Timer::SetTargetFPS(int target_fps) {
    if (target_fps <= 0) target_fps = 60;
    m_frameTime = 1.0 / target_fps;
}

void Timer::StartFrame() {
    auto now = Clock::now();
    std::chrono::duration<double> elapsed = now - m_frameStart;
    m_deltaTime = elapsed.count();
    m_frameStart = now; // 這一幀真正的開始時間

    // 防卡頓暴衝
    if (m_deltaTime > 0.1) {
        m_deltaTime = 0.1;
    }

    // 純FPS計算
    m_fpsCount++;
    m_fpsTimer += m_deltaTime;
    if (m_fpsTimer > 1.0) {
        m_currentFPS = m_fpsCount;
        m_fpsTimer -= 1.0;
        m_fpsCount = 0;
    }
}

void Timer::EndFrame() {
    // 將 double 轉回 Clock 預設單位 ns
    m_nextFrameTarget += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(m_frameTime));

    // 彈性前後幀時間
    auto now = Clock::now();
    if (now > m_nextFrameTarget) {
        m_nextFrameTarget = now;
        return; // 不等 下幀對齊
    }

    // sleep 保留 2ms 去做busy Wait
    while (Clock::now() < m_nextFrameTarget - std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Busy Wait
    while (Clock::now() < m_nextFrameTarget) {
        std::this_thread::yield();
    }
}

float Timer::GetDeltaTime() const {
    return static_cast<float>(m_deltaTime);
}

int Timer::GetFPS() const{
    return m_currentFPS;
}