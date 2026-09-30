#pragma once
#include <memory>
#include <vector>
#include <utility>

#include "entt/entt.hpp"

#include "system/Timer.h"
#include "system/InputManager.h"
#include "system/RenderHandle.h"


#include <DirectXMath.h>

struct GameContext {
    Timer&          m_timer;
    InputManager&   m_input;
};

struct Transform {
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 0.5f, 0.5f, 0.5f};             //都用四元數

    void SetRotationEuler(float pitchDegree,float yawDegree,float rollDegree) {
        float p = DirectX::XMConvertToRadians(pitchDegree);
        float y = DirectX::XMConvertToRadians(yawDegree);
        float r = DirectX::XMConvertToRadians(rollDegree);

        DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(p,y,r);
        DirectX::XMStoreFloat4(&rotation,q);
    };
};

struct Camera {
    float fovY = DirectX::XMConvertToRadians(60.0f);
    float nearZ = 0.1f;
    float farZ = 100.0f;
};

struct CameraControl {
    float deltaPitch = 0.0f;
    float deltaYaw = 0.0f; 
    float deltaRoll = 0.0f; 

    float pitchSpeed = 1.5f;
    float yawSpeed = 1.5f;
    float rollSpeed = 2.0f;
};

struct Speed {
    float x;
    float y;
    float z;
};

struct Lifetime {
    float time;
};

struct SceneRenderData;

class IScene {
public:
    IScene(GameContext& ctx) : m_ctx(ctx) {}
    virtual ~IScene() = default;
    virtual void Update() = 0;

    virtual void SetMeshes(std::vector<MeshHandle> meshes) = 0;
    virtual const SceneRenderData& GetRenderData() const = 0;
    
protected:
    std::vector<MeshHandle> m_meshes;
    GameContext& m_ctx;

private:
    
};

//目前是SetMeshes 直接溝通gpu rendersystem loadmesh後回傳的handle直接move到 m_meshes 
//之後要想看看怎樣改成chunk制
// 後續增加 texture handle vector 