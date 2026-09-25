#include "ResourceManager.h"

#include "GlbLoader.h"

void ResourceManager::Init() {
    InitializeConstantBuffer();
    InitializeStructureBuffer();
}

void ResourceManager::Update(const IScene& currentScene, uint32_t currentFrame) {
    UpdatePerFrameCB(currentScene, currentFrame);
    UpdateInstanceSB(currentScene, currentFrame);
}



MeshHandle ResourceManager::Load2DMesh(ID3D12GraphicsCommandList1* m_commandList) {
    MeshGpuResource gpuMesh{};
    gpuMesh.vertexCount = static_cast<uint32_t>(4);
    gpuMesh.indexCount = static_cast<uint32_t>(6);

    const uint64_t vbSize = gpuMesh.vertexCount * sizeof(Vertex);
    const uint64_t ibSize = gpuMesh.indexCount * sizeof(uint32_t);

    //建立default buffer
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = vbSize; // 緩衝區總大小
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
        "2D貼圖用default VB建立失敗"
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
        "2D貼圖用default IB建立失敗"
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
        "2D貼圖用upload VB建立失敗"
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
        "2D貼圖用upload IB建立失敗"
    );

    // 寫入資料進VB
    Vertex* pVertexDataBegin = nullptr;
    D3D12_RANGE readRange = { 0, 0 };				// CPU 不需要讀取這段記憶體 設為空
    gpuMesh.vertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    for (size_t i = 0; i < 1; i++)
    {
        size_t vOffset = i * 4;

        pVertexDataBegin[vOffset + 0] = Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f } }; // 左上 
        pVertexDataBegin[vOffset + 1] = Vertex{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } }; // 右上 
        pVertexDataBegin[vOffset + 2] = Vertex{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f } }; // 右下 
        pVertexDataBegin[vOffset + 3] = Vertex{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 1.0f } }; // 左下
    }
    gpuMesh.vertexUploadBuffer->Unmap(0, nullptr);

    // 寫入資料進IB
    uint32_t* pIndexDataBegin = nullptr;
    gpuMesh.indexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    for (size_t i = 0;i < 1; i++)
    {
        size_t vOffset = i * 4; // 每個物件佔用 4 個頂點
        size_t iOffset = i * 6; // 每個物件佔用 6 個索引

        // 三角形 1
        pIndexDataBegin[iOffset + 0] = static_cast<uint32_t>(vOffset + 0); //左上
        pIndexDataBegin[iOffset + 1] = static_cast<uint32_t>(vOffset + 1); //右上
        pIndexDataBegin[iOffset + 2] = static_cast<uint32_t>(vOffset + 2); //右下

        // 三角形 2
        pIndexDataBegin[iOffset + 3] = static_cast<uint32_t>(vOffset + 2); //右下
        pIndexDataBegin[iOffset + 4] = static_cast<uint32_t>(vOffset + 3); //左下
        pIndexDataBegin[iOffset + 5] = static_cast<uint32_t>(vOffset + 0); //左上
    }
    gpuMesh.indexUploadBuffer->Unmap(0, nullptr);

    //複製資料進default buffer
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
    gpuMesh.vertexBufferView.StrideInBytes = sizeof(Vertex);

    // 填寫IB View
    gpuMesh.indexBufferView.BufferLocation = gpuMesh.indexBuffer->GetGPUVirtualAddress();
    gpuMesh.indexBufferView.SizeInBytes = static_cast<uint32_t>(ibSize);
    gpuMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    MeshHandle handle{
    static_cast<uint32_t>(m_meshes.size())
    };

    m_meshes.push_back(std::move(gpuMesh));
    return handle;
}

MeshHandle ResourceManager::LoadMesh(const char* path, ID3D12GraphicsCommandList1* m_commandList) {
    MeshData meshData = LoadGlbMeshData(path);

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::runtime_error("GLB 沒有資料");
    }

    MeshGpuResource gpuMesh{};
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    //設定uint64_t 避免模型太大截斷
    const uint64_t vbSize = uint64_t(meshData.vertices.size()) * sizeof(MeshVertex);
    const uint64_t ibSize = uint64_t(meshData.indices.size()) * sizeof(uint32_t);
    if (vbSize > UINT32_MAX || ibSize > UINT32_MAX)
        throw std::runtime_error("Mesh buffer is too large");

    //建立default buffer
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

    MeshHandle handle{
        static_cast<uint32_t>(m_meshes.size())
    };

    m_meshes.push_back(std::move(gpuMesh));
    return handle;
}

void ResourceManager::TransitionMeshBuffer(std::vector<MeshGpuResource>meshes, ID3D12GraphicsCommandList1* m_commandList) {
    for (auto& mesh : meshes) {
        D3D12_RESOURCE_BARRIER barriers[2]{};

        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = mesh.vertexBuffer.Get();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = mesh.indexBuffer.Get();
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_commandList->ResourceBarrier(2, barriers);
    }
}

void ResourceManager::ClearUploadBuffer() {
    for (auto& mesh : m_meshes) {
        mesh.indexUploadBuffer.Reset();
        mesh.vertexUploadBuffer.Reset();
    }
}

const MeshGpuResource& ResourceManager::GetMesh(MeshHandle handle) const {
    return m_meshes.at(handle.id);
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
            m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer[f][i])
            );
            D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
            m_constantBuffer[f][i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvCpuAdress[f][i]));
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
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_structureBuffer[i])
        );
        D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
        m_structureBuffer[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_structureBufferCpuAddress[i]));
    }
}

void ResourceManager::UpdatePerFrameCB(const IScene& currentScene,uint32_t currentFrame){
    const float width = Common::InitialWindowWidth;
    const float height = Common::InitialWindowHeight;

    const auto eye = DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 1.0f);
    const auto target = DirectX::XMVectorZero();
    const auto up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    const auto view = DirectX::XMMatrixLookAtLH(eye, target, up);
    const auto proj = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(60.0f),
        width / height,
        0.1f,
        100.0f
    );

    PerFrameConstant cbData;
    DirectX::XMStoreFloat4x4(
        &cbData.ViewProj,
        DirectX::XMMatrixTranspose(view * proj)
    );
    cbData.CameraPos = { 0.0f, 0.0f, -5.0f };

    memcpy(m_cbvCpuAdress[currentFrame][0], &cbData, sizeof(cbData));
}

//之後改成看單子
void ResourceManager::UpdateInstanceSB(const IScene& currentScene,uint32_t currentFrame){
    memcpy(m_structureBufferCpuAddress[currentFrame],
        currentScene.InstanceDatas.data(),
        currentScene.InstanceDatas.size() * sizeof(InstanceData)
    );
}