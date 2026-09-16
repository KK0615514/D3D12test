#include "ResourceManager.h"

void ResourceManager::Init() {
    InitializeConstantBuffer();
    InitializeStructureBuffer();
}

void ResourceManager::Update(const IScene& currentScene, uint32_t currentFrame) {
    UpdatePerFrameCB(currentScene, currentFrame);
    UpdateInstanceSB(currentScene, currentFrame);
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
    float windowWidth = 1280.0f;
    float windowHeight = 720.0f;

    //寫死的camera
    DirectX::XMFLOAT2 camPos = { 0.0f, 0.0f };   // 相看著世界坐標的 (0,0)
    float camRotation = 0.0f;                    // 角度
    float camZoom = 1.0f;                        // 縮放倍率

    DirectX::XMMATRIX projMat = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, windowWidth, windowHeight, 0.0f, -1.0f, 1.0);
    DirectX::XMMATRIX viewMat = DirectX::XMMatrixIdentity();                        //單位矩陣
    DirectX::XMMATRIX viewProjMat = DirectX::XMMatrixMultiply(viewMat, projMat);    //矩陣乘法

    PerFrameConstant cbData;
    DirectX::XMStoreFloat4x4(&cbData.View, DirectX::XMMatrixTranspose(viewMat));
    DirectX::XMStoreFloat4x4(&cbData.Proj, DirectX::XMMatrixTranspose(projMat));
    DirectX::XMStoreFloat4x4(&cbData.ViewProj, DirectX::XMMatrixTranspose(viewProjMat));
    cbData.CameraPos = DirectX::XMFLOAT3(camPos.x, camPos.y, 0.0f);
    cbData.padding = 0.0f;

    //cbv裡面只有一個物件 不用對齊
    memcpy(m_cbvCpuAdress[currentFrame][0], &cbData, sizeof(cbData));
}

void ResourceManager::UpdateInstanceSB(const IScene& currentScene,uint32_t currentFrame){
    memcpy(m_structureBufferCpuAddress[currentFrame],
        currentScene.InstanceDatas.data(),
        currentScene.InstanceDatas.size() * sizeof(InstanceData)
    );
}