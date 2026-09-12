#pragma once
#include <chrono>
#include <thread>

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;                    //ns級時間點 透過大量計算精確時間點得出 deltatime 跟 FPS

class Timer {
public:
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

    void SetTargetFPS(int target_fps);
    void StartFrame();
    void EndFrame();

    float GetDeltaTime() const;
    int GetFPS() const;

    static Timer& GetInstance() {
        static Timer instance;
        return instance;
    }

private:
    TimePoint m_frameStart;
    TimePoint m_nextFrameTarget;
    double m_frameTime;     
    double m_deltaTime; 

    int m_fpsCount;
    double m_fpsTimer;
    int m_currentFPS;

    Timer(int target_fps = 60): 
        m_deltaTime(0.0),
        m_fpsCount(0),
        m_fpsTimer(0.0), 
        m_currentFPS(0) 
    {
        SetTargetFPS(target_fps);
        m_frameStart = Clock::now();
        m_nextFrameTarget = Clock::now();
    }
    ~Timer() = default;
};