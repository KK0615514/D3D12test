#include "ResourceManager.h"

#include <string>				// 使用 to_string
#include <cstring>				// 使用 std::memcpy

#include "system/RenderData.h"

void ResourceManager::Init() {
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



// test 拿DescriptorManager的 heap 然後在上面生資源
void ResourceManager::InitializeOceanHeight() {
    if (!m_descriptorManager) {
        throw std::logic_error("ResourceManager::InitializeOceanHeight : 未持有Descriptor Manager");
    }

    const auto srvSlot = m_descriptorManager->Allocate();
    const auto uavSlot = m_descriptorManager->Allocate();

    m_oceanHeightSrv = srvSlot.gpu;
    m_oceanHeightUav = uavSlot.gpu;

    // R32_FLOAT：每個 texel 儲存一個有正負值的高度。
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = OceanHeightResolution;
    desc.Height = OceanHeightResolution;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    Common::ThrowIfFailed(
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_oceanHeight)
        ),
        "ResourceManager::InitializeOceanHeight : Ocean height texture 建立失敗"
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_device->CreateShaderResourceView(
        m_oceanHeight.Get(),
        &srvDesc,
        srvSlot.cpu
    );


    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    m_device->CreateUnorderedAccessView(
        m_oceanHeight.Get(),
        nullptr,
        &uavDesc,
        uavSlot.cpu
    );
}
void ResourceManager::PrepareOceanHeight(
    ID3D12GraphicsCommandList1* directList,
    ID3D12RootSignature* computeRootSignature,
    ID3D12PipelineState* computePso,
    const OceanWaveParameters& parameters)
{
    // SRV 讀取 → UAV 寫入
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_oceanHeight.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    directList->ResourceBarrier(1, &barrier);

    directList->SetComputeRootSignature(computeRootSignature);
    directList->SetPipelineState(computePso);

    directList->SetComputeRootDescriptorTable(
        5,
        m_oceanHeightUav
    );

    directList->SetComputeRoot32BitConstants(
        6,
        4,
        &parameters,
        0
    );

    // HLSL 每組有 8 × 8 個 threads。
    const uint32_t groupCount = (OceanHeightResolution + 7) / 8;

    directList->Dispatch(groupCount, groupCount, 1);

    // UAV 寫入 → Vertex Shader SRV 讀取
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    directList->ResourceBarrier(1, &barrier);
}

MeshManager* ResourceManager::GetMeshManager() {
    return m_meshManager.get();
}

TextureManager* ResourceManager::GetTextureManager() {
    return m_textureManager.get();
}

DescriptorManager* ResourceManager::GetDescriptorManager() {
    return m_descriptorManager.get();
}