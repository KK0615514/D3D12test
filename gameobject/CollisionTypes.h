#pragma once
#include <cstdint>

enum CollisionCategory : uint16_t {
    PLAYER = 1 << 0, 
    ENEMY = 1 << 1, 
    BULLET = 1 << 2, 
};