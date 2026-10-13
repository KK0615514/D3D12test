#include "system/MeshManager.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#include "system/CommonUtils.h"
#include "system/GlbLoader.h"

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
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = vbSize;
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.Format = DXGI_FORMAT_UNKNOWN;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.vertexBuffer)
        ),
        "MeshManager::LoadOceanMesh : Ocean default VB 建立失敗"
    );

    // 建立 default IB
    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.indexBuffer)
        ),
        "MeshManager::LoadOceanMesh : Ocean default IB 建立失敗"
    );

    // 建立 upload VB
    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadVbDesc = vbDesc;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadVbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.vertexUploadBuffer)
        ),
        "MeshManager::LoadOceanMesh : Ocean upload VB 建立失敗"
    );

    // 建立 upload IB
    D3D12_RESOURCE_DESC uploadIbDesc = ibDesc;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadIbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.indexUploadBuffer)
        ),
        "MeshManager::LoadOceanMesh : Ocean upload IB 建立失敗"
    );

    // CPU 頂點資料寫入 upload VB
    D3D12_RANGE readRange{ 0, 0 };
    void* mappedVbData = nullptr;

    Common::ThrowIfFailed(
        gpuMesh.vertexUploadBuffer->Map(0, &readRange, &mappedVbData),
        "MeshManager::LoadOceanMesh : Ocean VB Map 失敗"
    );
    std::memcpy(
        mappedVbData,
        meshData.vertices.data(),
        static_cast<size_t>(vbSize)
    );
    gpuMesh.vertexUploadBuffer->Unmap(0, nullptr);

    // CPU 索引資料寫入 upload IB
    void* mappedIbData = nullptr;
    Common::ThrowIfFailed(
        gpuMesh.indexUploadBuffer->Map(0, &readRange, &mappedIbData),
        "MeshManager::LoadOceanMesh : Ocean IB Map 失敗"
    );
    std::memcpy(
        mappedIbData,
        meshData.indices.data(),
        static_cast<size_t>(ibSize)
    );
    gpuMesh.indexUploadBuffer->Unmap(0, nullptr);

    // 記錄 upload → default 的複製指令
    copyList->CopyBufferRegion(
        gpuMesh.vertexBuffer.Get(),
        0,
        gpuMesh.vertexUploadBuffer.Get(),
        0,
        vbSize
    );

    copyList->CopyBufferRegion(
        gpuMesh.indexBuffer.Get(),
        0,
        gpuMesh.indexUploadBuffer.Get(),
        0,
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
    MeshHandle handle{
        static_cast<uint32_t>(m_meshes.size())
    };

    m_meshes.push_back(std::move(gpuMesh));

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

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.vertexBuffer)
        ),
        "MeshManager::LoadMesh : Mesh用default VB建立失敗"
    );

    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.indexBuffer)
        ),
        "MeshManager::LoadMesh : Mesh用default IB建立失敗"
    );

    //建立upload用buffer
    D3D12_RESOURCE_DESC uploadVbDesc = vbDesc;

    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadVbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.vertexUploadBuffer)
        ),
        "MeshManager::LoadMesh : Mesh用upload VB建立失敗"
    );

    D3D12_RESOURCE_DESC uploadIbDesc = ibDesc;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadIbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.indexUploadBuffer)
        ),
        "MeshManager::LoadMesh : Mesh用upload IB建立失敗"
    );

    //上傳資料 進upload vb
    void* mappedVbData = nullptr;
    D3D12_RANGE readRange = { 0,0 };
    Common::ThrowIfFailed(
        gpuMesh.vertexUploadBuffer->Map(0, &readRange, &mappedVbData),
        "MeshManager::LoadMesh : Mesh VB 資料上傳失敗"
    );
    memcpy(mappedVbData,
        meshData.vertices.data(),
        vbSize
    );
    gpuMesh.vertexUploadBuffer->Unmap(0, nullptr);

    //上傳資料 進upload ib
    void* mappedIbData = nullptr;
    Common::ThrowIfFailed(
        gpuMesh.indexUploadBuffer->Map(0, &readRange, &mappedIbData),
        "MeshManager::LoadMesh : Mesh IB 資料上傳失敗"
    );

    memcpy(mappedIbData,
        meshData.indices.data(),
        ibSize
    );
    gpuMesh.indexUploadBuffer->Unmap(0, nullptr);

    //複製資料 進default vb ib
    copyList->CopyBufferRegion(
        gpuMesh.vertexBuffer.Get(),
        0,
        gpuMesh.vertexUploadBuffer.Get(),
        0,
        vbSize
    );
    copyList->CopyBufferRegion(
        gpuMesh.indexBuffer.Get(),
        0,
        gpuMesh.indexUploadBuffer.Get(),
        0,
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
    MeshHandle handle{
        static_cast<uint32_t>(m_meshes.size())
    };

    m_meshes.push_back(std::move(gpuMesh));
    m_meshCache.emplace(key, handle);

    return MeshLoadResult{ handle,true };
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
    for (auto& mesh : m_meshes) {
        mesh.indexUploadBuffer.Reset();
        mesh.vertexUploadBuffer.Reset();
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