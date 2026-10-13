#include "ResourceManager.h"

#include <string>				// 使用 to_string
#include <cstring>				// 使用 std::memcpy

#include "system/RenderData.h"

void ResourceManager::Init() {
    m_heapManager = std::make_unique<HeapManager>(m_device.Get());
    m_heapManager->Init();

    m_descriptorManager     = std::make_unique<DescriptorManager>(m_device.Get());
    m_descriptorManager->Init();

    m_meshManager           = std::make_unique<MeshManager>(m_device.Get());
    m_textureManager        = std::make_unique<TextureManager>(m_device.Get(), m_descriptorManager.get());

    InitializeConstantBuffer();
    InitializeStructureBuffer();
}

void ResourceManager::Update(const SceneRenderData& data, uint32_t currentFrame) {
    UpdatePerFrameCB(data, currentFrame);
    UpdateInstanceSB(data, currentFrame);
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
                "ResourceManager::InitializeConstantBuffer : CB建立失敗"
            );
            D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
            Common::ThrowIfFailed(
                m_constantBuffer[f][i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvCpuAdress[f][i])),
                "ResourceManager::InitializeConstantBuffer : CB 用map建立失敗"
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
            "ResourceManager::InitializeStructureBuffer : SB 建立失敗"
        );
        D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
        Common::ThrowIfFailed(
            m_structureBuffer[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_structureBufferCpuAddress[i])),
            "ResourceManager::InitializeStructureBuffer : SB 用指標map失敗"
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
            "ResourceManager::UpdateInstanceSB : Instance count 超過最大容量: " +
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


HeapManager* ResourceManager::GetHeapManager() {
    return m_heapManager.get();
}

DescriptorManager* ResourceManager::GetDescriptorManager() {
    return m_descriptorManager.get();
}

MeshManager* ResourceManager::GetMeshManager() {
    return m_meshManager.get();
}

TextureManager* ResourceManager::GetTextureManager() {
    return m_textureManager.get();
}
