#include "system/MeshManager.h"

#include <cstring>
#include <stdexcept>
#include <utility>
#include <cassert>

#include "system/CommonUtils.h"
#include "system/GlbLoader.h"

ComPtr<ID3D12Resource> MeshManager::CreateDefaultBuffer(const D3D12_RESOURCE_DESC& desc, HeapAllocation& allocation){
    if (!m_device || !m_heapManager) {
        throw std::logic_error("MeshManager::CreateDefaultBuffer : 缺少 Device 或 HeapManager");
    }
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || desc.Width == 0){
        throw std::invalid_argument("MeshManager::CreateDefaultBuffer : 需要有效的 Buffer 描述");
    }

    const HeapAllocation allocated = m_heapManager->Allocate(
        desc,
        D3D12_HEAP_TYPE_DEFAULT
    );

    ComPtr<ID3D12Resource> resource;

    Common::ThrowIfFailed(
        m_device->CreatePlacedResource(
            m_heapManager->GetHeap(allocated.heapId),
            allocated.offset,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&resource)
        ),
        "MeshManager::CreateDefaultBuffer : Placed buffer 建立失敗"
    );

    allocation = allocated;
    return resource;
}

// cpu資料 memcpy進gpu
UploadSlice MeshManager::StageUploadData(const void* sourceData,uint64_t size) {
    if (!m_uploadManager) {
        throw std::logic_error("MeshManager::StageUploadData : 缺少 UploadManager");
    }
    if (!sourceData || size == 0) {
        throw std::invalid_argument("MeshManager::StageUploadData : 無效來源資料或 Size");
    }

    const UploadSlice slice = m_uploadManager->Allocate(
        size,
        4
    );

    std::memcpy(
        slice.cpuAddress,
        sourceData,
        static_cast<size_t>(size)
    );

    return slice;
}



MeshHandle MeshManager::LoadOceanMesh(const OceanMeshData& meshData, ID3D12GraphicsCommandList1* copyList) {
    // 檢查資料合不合規
    if (!m_device || !copyList) {
        throw std::invalid_argument("MeshManager::LoadOceanMesh : 無效 Device 或 CopyList");
    }
    if (copyList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
        throw std::invalid_argument("MeshManager::LoadOceanMesh : 需要 Copy CommandList");
    }
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::invalid_argument("MeshManager::LoadOceanMesh : 頂點或索引資料為空");
    }
    if (m_meshes.size() >= UINT32_MAX) {
        throw std::overflow_error("MeshManager::LoadOceanMesh : Handle 數量超出上限");
    }
    if (meshData.vertices.size() > UINT32_MAX / sizeof(OceanVertex) || meshData.indices.size() > UINT32_MAX / sizeof(uint32_t)) {
        throw std::overflow_error("MeshManager::LoadOceanMesh : 超出buffer的最大上限");
    }

    const uint64_t vbSize = uint64_t(meshData.vertices.size()) * sizeof(OceanVertex);
    const uint64_t ibSize = uint64_t(meshData.indices.size()) * sizeof(uint32_t);

    MeshGpuResource gpuMesh{};
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    // 建立 default VB
    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = vbSize;
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.Format = DXGI_FORMAT_UNKNOWN;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    // 建立 default VB
    gpuMesh.vertexBuffer = CreateDefaultBuffer(
        vbDesc,
        gpuMesh.vertexAllocation
    );

    // 建立 default IB
    gpuMesh.indexBuffer = CreateDefaultBuffer(
        ibDesc,
        gpuMesh.indexAllocation
    );

    // 建立 upload VB
    auto vertexUploadBuffer = StageUploadData(
        meshData.vertices.data(),
        vbSize
    );

    // 建立 upload IB
    auto indexUploadBuffer = StageUploadData(
        meshData.indices.data(),
        ibSize
    );

    // 填寫 VB View
    gpuMesh.vertexBufferView.BufferLocation = gpuMesh.vertexBuffer->GetGPUVirtualAddress();
    gpuMesh.vertexBufferView.SizeInBytes = static_cast<uint32_t>(vbSize);
    gpuMesh.vertexBufferView.StrideInBytes = sizeof(OceanVertex);

    // 填寫 IB View
    gpuMesh.indexBufferView.BufferLocation = gpuMesh.indexBuffer->GetGPUVirtualAddress();
    gpuMesh.indexBufferView.SizeInBytes = static_cast<uint32_t>(ibSize);
    gpuMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // 保留資源並取得 Handle
    const MeshHandle handle{
        static_cast<uint32_t>(m_meshes.size())
    };

    const size_t meshCount = m_meshes.size();
    const size_t pendingCount = m_pendingUploadBuffers.size();

    // 此處尚未錄製這筆 mesh 的 copy。
    // 保存失敗時，只移除這筆新增的紀錄。
    try {
        m_pendingUploadBuffers.push_back(std::move(vertexUploadBuffer));
        m_pendingUploadBuffers.push_back(std::move(indexUploadBuffer));

        m_meshes.push_back(std::move(gpuMesh));
    }
    catch (...) {
        if (m_meshes.size() > meshCount) {
            m_meshes.pop_back();
        }
        while (m_pendingUploadBuffers.size() > pendingCount) {
            m_pendingUploadBuffers.pop_back();
        }
        throw;
    }

    // DEFAULT 與 UPLOAD 資源都已由 manager 持有。
    const auto& storedMesh  = m_meshes[handle.id];
    const auto& vertexSlice = m_pendingUploadBuffers[pendingCount];
    const auto& indexSlice  = m_pendingUploadBuffers[pendingCount + 1];

    copyList->CopyBufferRegion(
        storedMesh.vertexBuffer.Get(),
        0,
        vertexSlice.resource,
        vertexSlice.offset,
        vbSize
    );

    copyList->CopyBufferRegion(
        storedMesh.indexBuffer.Get(),
        0,
        indexSlice.resource,
        indexSlice.offset,
        ibSize
    );

    return handle;
}

MeshLoadResult MeshManager::LoadMesh(const char* name, ID3D12GraphicsCommandList1* copyList) {
    if (!m_device || !name || name[0] == '\0' || !copyList) {
        throw std::invalid_argument("MeshManager::LoadMesh : 無效 Device、名稱或 CopyList");
    }
    if (copyList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
        throw std::invalid_argument("MeshManager::LoadMesh : 需要 Copy CommandList");
    }

    const std::string key = name;
    const auto cachedHandle = m_meshCache.find(key);

    // 檢查Handle來確認是否以載入 有則回傳
    if (cachedHandle != m_meshCache.end()) {
        return MeshLoadResult{ cachedHandle->second, false};
    }

    if (m_meshes.size() >= UINT32_MAX) {
        throw std::overflow_error("MeshManager::LoadMesh : Handle 數量超出上限");
    }

    const std::string path = "asset/model/" + key + ".glb";
    MeshData meshData = LoadGlbMeshData(path.c_str());

    // 檢查GLB回傳是否為空
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::runtime_error("MeshManager::LoadMesh : GLB 沒有資料");
    }

    // 檢查VB IB大小是否合規
    if (meshData.vertices.size() > UINT32_MAX / sizeof(MeshVertex) ||
        meshData.indices.size() > UINT32_MAX / sizeof(uint32_t)){
        throw std::overflow_error("MeshManager::LoadMesh : VB 或 IB 大小超出上限");
    }

    // 設定uint64_t 避免模型太大截斷
    const uint64_t vbSize = uint64_t(meshData.vertices.size()) * sizeof(MeshVertex);
    const uint64_t ibSize = uint64_t(meshData.indices.size()) * sizeof(uint32_t);

    MeshGpuResource gpuMesh{};
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    // 建立default buffer
    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = vbSize;
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.Format = DXGI_FORMAT_UNKNOWN;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    gpuMesh.vertexBuffer = CreateDefaultBuffer(
        vbDesc,
        gpuMesh.vertexAllocation
    );

    gpuMesh.indexBuffer = CreateDefaultBuffer(
        ibDesc,
        gpuMesh.indexAllocation
    );

    auto vertexUploadBuffer = StageUploadData(
        meshData.vertices.data(),
        vbSize
    );

    auto indexUploadBuffer = StageUploadData(
        meshData.indices.data(),
        ibSize
    );

    // 填寫VB View
    gpuMesh.vertexBufferView.BufferLocation = gpuMesh.vertexBuffer->GetGPUVirtualAddress();
    gpuMesh.vertexBufferView.SizeInBytes = static_cast<uint32_t>(vbSize);
    gpuMesh.vertexBufferView.StrideInBytes = sizeof(MeshVertex);

    // 填寫IB View
    gpuMesh.indexBufferView.BufferLocation = gpuMesh.indexBuffer->GetGPUVirtualAddress();
    gpuMesh.indexBufferView.SizeInBytes = static_cast<uint32_t>(ibSize);
    gpuMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // 目前只追加 不刪除或重排 所以使用size來設計的Handle 除此之外跟meshCache無依賴關係  
    const MeshHandle handle{
        static_cast<uint32_t>(m_meshes.size())
    };

    const size_t meshCount = m_meshes.size();
    const size_t pendingCount = m_pendingUploadBuffers.size();

    // 所有可能配置記憶體的保存／註冊操作，放在 copy 之前。
    try {
        m_pendingUploadBuffers.push_back(
            std::move(vertexUploadBuffer)
        );

        m_pendingUploadBuffers.push_back(
            std::move(indexUploadBuffer)
        );

        m_meshes.push_back(std::move(gpuMesh));

        if (!m_meshCache.emplace(key, handle).second) {
            throw std::logic_error("MeshManager::LoadMesh : 名稱已存在");
        }
    }
    catch (...) {
        if (m_meshes.size() > meshCount) {
            m_meshes.pop_back();
        }

        while (m_pendingUploadBuffers.size() > pendingCount) {
            m_pendingUploadBuffers.pop_back();
        }

        throw;
    }

    const auto& storedMesh  = m_meshes[handle.id];
    const auto& vertexSlice = m_pendingUploadBuffers[pendingCount];
    const auto& indexSlice  = m_pendingUploadBuffers[pendingCount + 1];

    copyList->CopyBufferRegion(
        storedMesh.vertexBuffer.Get(),
        0,
        vertexSlice.resource,
        vertexSlice.offset,
        vbSize
    );

    copyList->CopyBufferRegion(
        storedMesh.indexBuffer.Get(),
        0,
        indexSlice.resource,
        indexSlice.offset,
        ibSize
    );

    return MeshLoadResult{ handle, true };
}

void MeshManager::TransitionMeshBuffers(MeshHandle handle, ID3D12GraphicsCommandList1* directList) {
    // 檢查是否是Direct Queue
    if (!directList ||  directList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT){
        throw std::invalid_argument("MeshManager::TransitionMeshBuffers : 需要有效的 Direct CommandList");
    }

    const MeshGpuResource* meshPtr = TryGetMesh(handle);

    if (!meshPtr) {
        throw std::runtime_error(
            "MeshManager::TransitionMeshBuffers : 錯誤的handle或是buffer"
        );
    }

    const MeshGpuResource& mesh = *meshPtr;

    D3D12_RESOURCE_BARRIER barriers[2]{};

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = mesh.vertexBuffer.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = mesh.indexBuffer.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    directList->ResourceBarrier(2, barriers);

}

void MeshManager::ClearUploadBuffers() {
    m_pendingUploadBuffers.clear();
}



MeshUploadCheckpoint MeshManager::GetUploadCheckpoint() const{
    return MeshUploadCheckpoint{
        m_meshes.size(),
        m_pendingUploadBuffers.size()
    };
}

void MeshManager::RollbackUpload(const MeshUploadCheckpoint& checkpoint){
    assert(
        checkpoint.meshCount <= m_meshes.size() &&
        checkpoint.pendingCount <= m_pendingUploadBuffers.size() &&
        "MeshManager::RollbackUpload : 無效 checkpoint"
    );

    // 只刪除本批次新增的名稱。
    for (auto it = m_meshCache.begin();it != m_meshCache.end(); ) {
        if (it->second.id >= checkpoint.meshCount) {
            it = m_meshCache.erase(it);
        }
        else {
            ++it;
        }
    }

    while (m_meshes.size() > checkpoint.meshCount) {
        m_meshes.pop_back();
    }

    while (m_pendingUploadBuffers.size() > checkpoint.pendingCount) {
        m_pendingUploadBuffers.pop_back();
    }
}



const MeshGpuResource* MeshManager::TryGetMesh(MeshHandle handle) const {
    // 檢查handle是否超出邊界
    if (handle.id >= m_meshes.size()) {
        return nullptr;
    }
    const auto& mesh = m_meshes[handle.id];
    // 檢查VB IB 是否為空
    if (!mesh.vertexBuffer || !mesh.indexBuffer) {
        return nullptr;
    }

    return &mesh;
}