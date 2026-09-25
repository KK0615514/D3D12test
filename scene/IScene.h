#pragma once
#include <memory>
#include <vector>
#include "entt/entt.hpp"

#include "system/Timer.h"
#include "system/InputManager.h"

//看情況調整
#include "ecs_component/ShaderStructureBuffer.h" 
#include "ecs_component/ShaderConstantBuffer.h" 


struct Transform {
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

struct Camera {
    float fovY{};
    float nearZ{};
    float farZ{};
};

class IScene {
public:
    //之後改成填單子
    std::vector<InstanceData> InstanceDatas;
    uint32_t instanceCounts;

public:
    virtual ~IScene() = default;
    virtual void Update() = 0;

};