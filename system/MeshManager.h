#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>              // uint32_t等
#include <string>               // 實作asset path cache
#include <unordered_map>        // 實作asset path cache
#include <vector>

#include "system/MeshData.h"
#include "system/RenderHandle.h"

using Microsoft::WRL::ComPtr;

// 物件資料
struct MeshGpuResource {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;

    ComPtr<ID3D12Resource> vertexUploadBuffer;
    ComPtr<ID3D12Resource> indexUploadBuffer;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};

    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;
};

// 檢查Mesh是否是新載入的
struct MeshLoadResult {
    MeshHandle handle;
    bool newlyCreated;
};

class MeshManager {
public:
    MeshManager(ID3D12Device4* device) : m_device(device) {}
    ~MeshManager() = default;

    MeshHandle LoadOceanMesh(const OceanMeshData& meshData, ID3D12GraphicsCommandList1* copyList);
    MeshLoadResult LoadMesh(const char* name, ID3D12GraphicsCommandList1* copyList);
    void TransitionMeshBuffer(MeshHandle handle, ID3D12GraphicsCommandList1* directList);
    void ClearUploadBuffer();

    const MeshGpuResource* TryGetMesh(MeshHandle handle) const;     //採用指標 方便檢測 因為可以nullptr回傳

private:
    Microsoft::WRL::ComPtr<ID3D12Device4> m_device;

    std::vector<MeshGpuResource> m_meshes;                          // 實際持有資源
    std::unordered_map<std::string, MeshHandle> m_meshCache;        // 對照表 檢查重複載入用
};