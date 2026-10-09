#include "ResourceManager.h"

#include "system/RenderData.h"
#include "system/GlbLoader.h"

void ResourceManager::Init() {
    InitializeConstantBuffer();
    InitializeStructureBuffer();
}

void ResourceManager::Update(const SceneRenderData& data, uint32_t currentFrame) {
    UpdatePerFrameCB(data, currentFrame);
    UpdateInstanceSB(data, currentFrame);
}



MeshHandle ResourceManager::LoadOceanMesh(const OceanMeshData& meshData, ID3D12GraphicsCommandList1* copyList)
{
    // 檢查資料合不合規
    if (!copyList || meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::invalid_argument("UploadOceanMesh: 無效輸入");
    }
    if (meshData.vertices.size() > UINT32_MAX / sizeof(OceanVertex) || meshData.indices.size() > UINT32_MAX / sizeof(uint32_t)) {
        throw std::overflow_error("UploadOceanMesh: 超出buffer的最大上限");
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
            IID_PPV_ARGS(&gpuMesh.vertexBuffer)),
        "Ocean default VB 建立失敗"
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
            IID_PPV_ARGS(&gpuMesh.indexBuffer)),
        "Ocean default IB 建立失敗"
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
            IID_PPV_ARGS(&gpuMesh.vertexUploadBuffer)),
        "Ocean upload VB 建立失敗"
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
            IID_PPV_ARGS(&gpuMesh.indexUploadBuffer)),
        "Ocean upload IB 建立失敗"
    );

    // CPU 頂點資料寫入 upload VB
    D3D12_RANGE readRange{ 0, 0 };
    void* mappedVbData = nullptr;

    Common::ThrowIfFailed(
        gpuMesh.vertexUploadBuffer->Map(0, &readRange, &mappedVbData),
        "Ocean VB Map 失敗"
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
        "Ocean IB Map 失敗"
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

MeshLoadResult ResourceManager::LoadMesh(const char* path, ID3D12GraphicsCommandList1* m_commandList) {
    const std::string key = path;
    const auto cachedHandle = m_meshCache.find(key);

    // 檢查Handle來確認是否以載入 有則回傳
    if (cachedHandle != m_meshCache.end()) {
        return MeshLoadResult{ cachedHandle->second,false };
    }

    MeshData meshData = LoadGlbMeshData(path);

    // 檢查GLB回傳是否為空
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::runtime_error("LoadMesh : GLB 沒有資料");
    }

    MeshGpuResource gpuMesh{};
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    // 設定uint64_t 避免模型太大截斷
    const uint64_t vbSize = uint64_t(meshData.vertices.size()) * sizeof(MeshVertex);
    const uint64_t ibSize = uint64_t(meshData.indices.size()) * sizeof(uint32_t);
    if (vbSize > UINT32_MAX || ibSize > UINT32_MAX) {
        throw std::runtime_error("LoadMesh : Mesh buffer 用的VB IB 溢位UINT32");
    }
        

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
            IID_PPV_ARGS(&gpuMesh.vertexBuffer)),
        "Mesh用default VB建立失敗"
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
            IID_PPV_ARGS(&gpuMesh.indexBuffer)),
        "Mesh用default IB建立失敗"
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
            IID_PPV_ARGS(&gpuMesh.vertexUploadBuffer)),
        "Mesh用upload VB建立失敗"
    );

    D3D12_RESOURCE_DESC uploadIbDesc = ibDesc;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadIbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gpuMesh.indexUploadBuffer)),
        "Mesh用upload IB建立失敗"
    );

    //上傳資料 進upload vb
    void* mappedVbData = nullptr;
    D3D12_RANGE readRange = { 0,0 };
    Common::ThrowIfFailed(
        gpuMesh.vertexUploadBuffer->Map(0, &readRange, &mappedVbData),
        "Mesh VB 資料上傳失敗"
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
        "Mesh IB 資料上傳失敗"
    );

    memcpy(mappedIbData,
        meshData.indices.data(),
        ibSize
    );
    gpuMesh.indexUploadBuffer->Unmap(0, nullptr);

    //複製資料 進default vb ib
    m_commandList->CopyBufferRegion(
        gpuMesh.vertexBuffer.Get(),
        0,
        gpuMesh.vertexUploadBuffer.Get(),
        0,
        vbSize
    );
    m_commandList->CopyBufferRegion(
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

    return MeshLoadResult{handle,true};
}

void ResourceManager::TransitionMeshBuffer(MeshHandle handle, ID3D12GraphicsCommandList1* m_commandList) {
    const MeshGpuResource* meshPtr = TryGetMesh(handle);

    if (!meshPtr) {
        throw std::runtime_error(
            "TransitionMeshBuffer : 錯誤的handle或是buffer"
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

    m_commandList->ResourceBarrier(2, barriers);

}

void ResourceManager::ClearUploadBuffer() {
    for (auto& mesh : m_meshes) {
        mesh.indexUploadBuffer.Reset();
        mesh.vertexUploadBuffer.Reset();
    }
}

const MeshGpuResource* ResourceManager::TryGetMesh(MeshHandle handle) const {
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

 

void ResourceManager::InitializeConstantBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    uint32_t bufferSize = 4 * 1024; //每個cbv 上限64kb 效能為主4kb
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize; // 必須對齊256
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (uint32_t f = 0;f < FrameCount;++f)
    {
        for (uint32_t i = 0;i < MaxCBVsPerFrame;++i)
        {
            Common::ThrowIfFailed(
                m_device->CreateCommittedResource(
                    &heapProps,
                    D3D12_HEAP_FLAG_NONE,
                    &resourceDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&m_constantBuffer[f][i])
                ),
                "CB建立失敗"
            );
            D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
            Common::ThrowIfFailed(
                m_constantBuffer[f][i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvCpuAdress[f][i])),
                "CB 用map建立失敗"
            );

        }
    }
}

void ResourceManager::InitializeStructureBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;                            //未來改成default 搭配一個upload buffer來每幀上傳物件資料
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;                                               // 0 表示預設對齊（64KB）
    bufferDesc.Width = sizeof(InstanceData) * MAX_ELEMENTS;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured Buffer必為UNKNOWN
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (uint32_t i = 0; i < FrameCount; i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_structureBuffer[i])
            ),
            "SB 建立失敗"
        );
        D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
        Common::ThrowIfFailed(
            m_structureBuffer[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_structureBufferCpuAddress[i])),
            "SB 用指標map失敗"
        );
        
    }
}

void ResourceManager::UpdatePerFrameCB(const SceneRenderData& data, uint32_t currentFrame) {
    memcpy(m_cbvCpuAdress[currentFrame][0], 
        &data.cameraData,
        sizeof(data.cameraData)
    );
}

void ResourceManager::UpdateInstanceSB(const SceneRenderData& data, uint32_t currentFrame){
    // 檢查當前幀的Instance count是否超過 SB最大容量
    if (data.instanceDatas.size() > MAX_ELEMENTS) {
        throw std::runtime_error(
            "UpdateInstanceSB : Instance count 超過最大容量: " +
            std::to_string(MAX_ELEMENTS)
        );
    }

    // 檢查SB是否為空
    if (data.instanceDatas.empty()) {
        return;
    }
    
    memcpy(m_structureBufferCpuAddress[currentFrame],
        data.instanceDatas.data(),
        data.instanceDatas.size() * sizeof(InstanceData)
    );
}