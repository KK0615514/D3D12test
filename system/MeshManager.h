#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>              // uint32_t等
#include <string>               // 實作asset path cache
#include <unordered_map>        // 實作asset path cache
#include <vector>

#include "system/HeapManager.h"
#include "system/UploadManager.h"

#include "system/MeshData.h"
#include "system/RenderHandle.h"

using Microsoft::WRL::ComPtr;

// 物件資料
struct MeshGpuResource {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;

    HeapAllocation vertexAllocation{};
    HeapAllocation indexAllocation{};

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

struct MeshUploadCheckpoint {
    std::size_t meshCount = 0;
    std::size_t pendingCount = 0;
};



class MeshManager {
public:
    MeshManager(ID3D12Device4* device, HeapManager* heapManager, UploadManager* uploadManager)
        : m_device(device),m_heapManager(heapManager),m_uploadManager(uploadManager) {}
    ~MeshManager() = default;

    MeshHandle LoadOceanMesh(const OceanMeshData& meshData, ID3D12GraphicsCommandList1* copyList);
    MeshLoadResult LoadMesh(const char* name, ID3D12GraphicsCommandList1* copyList);
    void TransitionMeshBuffers(MeshHandle handle, ID3D12GraphicsCommandList1* directList);
    void ClearUploadBuffers();

    MeshUploadCheckpoint GetUploadCheckpoint() const;
    void RollbackUpload(const MeshUploadCheckpoint& checkpoint);    // 只撤回 checkpoint 之後新增、尚未提交的紀錄。

    const MeshGpuResource* TryGetMesh(MeshHandle handle) const;     //採用指標 方便檢測 因為可以nullptr回傳

private:
    Microsoft::WRL::ComPtr<ID3D12Device4> m_device;
    HeapManager* m_heapManager = nullptr;
    UploadManager* m_uploadManager = nullptr;

    std::vector<MeshGpuResource> m_meshes;                          // 實際持有資源
    std::unordered_map<std::string, MeshHandle> m_meshCache;        // 對照表 檢查重複載入用

    std::vector<UploadSlice> m_pendingUploadBuffers;                // Upload用的buffer 實際資源由Upload Manager持有 

    ComPtr<ID3D12Resource> CreateDefaultBuffer(const D3D12_RESOURCE_DESC& desc, HeapAllocation& allocation);
    UploadSlice StageUploadData(const void* sourceData,uint64_t size);
};