#pragma once
#include <cstdint>
#include <random>

namespace Xoshiro128
{
    // Xoshiro128++ 演算法
    // 呼叫這個函式，就能以極致效能拿到 0.0f ~ 1.0f 的隨機數
    inline float Random() {
        // thread_local 確保了多執行緒並行（ECS）時的絕對安全與高效
        // 每個執行緒第一次進來時，會自動用硬體隨機數初始化自己的狀態
        thread_local uint32_t s[4] = {
            std::random_device{}(), std::random_device{}(),
            std::random_device{}(), std::random_device{}()
        };

        // Xoshiro128++ 核心演算法：純位移與加法，沒有任何除法
        const uint32_t result = ((s[0] + s[3]) << 7) | ((s[0] + s[3]) >> 25); // rotl(s[0]+s[3], 7)
        const uint32_t t = s[1] << 9;

        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t;
        s[3] = (s[3] << 11) | (s[3] >> 21); // rotl(s[3], 11)

        // 將 32 位元整數映射到 [0.0f, 1.0f) 區間
        return static_cast<float>(result >> 8) / 16777216.0f;
    }
    inline float RandomRange(float min, float max) {
        return min + Random() * (max - min);
    }
}