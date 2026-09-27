#pragma once
#include <memory>
#include <vector>
#include <utility>

#include "entt/entt.hpp"

#include "system/Timer.h"
#include "system/InputManager.h"
#include "system/RenderHandle.h"


#include <DirectXMath.h>
struct Transform {
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 0.5f, 0.5f, 0.5f};
};

struct Camera {
    float fovY{};
    float nearZ{};
    float farZ{};
};

struct Speed {
    float x;
    float y;
    float z;
};

struct SceneRenderData;

class IScene {
public:
    virtual ~IScene() = default;
    virtual void Update() = 0;

    virtual void SetMeshes(std::vector<MeshHandle> meshes) = 0;
    virtual const SceneRenderData& GetRenderData() const = 0;
    
protected:
    std::vector<MeshHandle> m_meshes;
};

//目前是SetMeshes 直接溝通gpu rendersystem loadmesh後回傳的handle直接move到 m_meshes 
//之後要想看看怎樣改成chunk制