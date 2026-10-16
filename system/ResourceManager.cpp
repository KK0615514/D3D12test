#include "ResourceManager.h"

#include <string>				// 使用 to_string
#include <cstring>				// 使用 std::memcpy
#include <stdexcept>

#include "system/RenderData.h"

void ResourceManager::Init() {
    if (!m_device || !m_adapter) {
        throw std::invalid_argument("ResourceManager::Init : 無效 Device 或 Adapter");
    }
    if (m_heapManager || m_uploadManager || m_descriptorManager || m_meshManager || m_textureManager) {
        throw std::logic_error("ResourceManager::Init : 已初始化或已有部分初始化的資源");
    }

    m_heapManager = std::make_unique<HeapManager>(m_device.Get(), m_adapter.Get());
    m_heapManager->Init();

    m_uploadManager = std::make_unique<UploadManager>(m_device.Get(), m_heapManager.get());
    m_uploadManager->Init();

    m_descriptorManager     = std::make_unique<DescriptorManager>(m_device.Get());
    m_descriptorManager->Init();

    m_meshManager           = std::make_unique<MeshManager>(m_device.Get(), m_heapManager.get(), m_uploadManager.get());
    m_textureManager        = std::make_unique<TextureManager>(m_device.Get(), m_heapManager.get(), m_uploadManager.get(), m_descriptorManager.get());

    InitConstantBuffers();
    InitStructuredBuffers();
}

void ResourceManager::Update(const SceneRenderData& data, uint32_t currentFrame) {
    if (currentFrame >= FrameCount) {
        throw std::out_of_range("ResourceManager::Update : Frame index 超出範圍");
    }

    UpdatePerFrameCB(data, currentFrame);
    UpdateInstanceSB(data, currentFrame);
}



void ResourceManager::BeginUploadBatch() {
    if (m_uploadBatchActive) {
        throw std::logic_error("ResourceManager::BeginUploadBatch : 已有進行中的批次");
    }
    if (!m_meshManager ||
        !m_textureManager ||
        !m_uploadManager ||
        !m_descriptorManager) {
        throw std::logic_error("ResourceManager::BeginUploadBatch : Managers 尚未初始化");
    }

    m_meshUploadCheckpoint = m_meshManager->GetUploadCheckpoint();
    m_textureUploadCheckpoint = m_textureManager->GetUploadCheckpoint();
    m_descriptorUploadCheckpoint = m_descriptorManager->GetAllocationCount();

    m_uploadManager->BeginBatch();

    m_uploadSubmissionStarted = false;
    m_uploadBatchActive = true;
}

void ResourceManager::MarkUploadSubmissionStarted() {
    if (!m_uploadBatchActive || m_uploadSubmissionStarted) {
        throw std::logic_error("ResourceManager::MarkUploadSubmissionStarted : 無效批次狀態");
    }

    m_uploadSubmissionStarted = true;
}

void ResourceManager::CommitUploadBatch(uint64_t copyFenceValue) {
    if (!m_uploadBatchActive || !m_uploadSubmissionStarted) {
        throw std::logic_error("ResourceManager::CommitUploadBatch : 無效批次狀態");
    }

    m_uploadManager->MarkSubmitted(copyFenceValue);

    // 切片 resource 由 UploadManager 持有。
    // 提交後可清除 manager 端的切片紀錄。
    m_meshManager->ClearUploadBuffers();
    m_textureManager->ClearUploadBuffers();

    m_uploadBatchActive = false;
    m_uploadSubmissionStarted = false;
}

void ResourceManager::RollbackUploadBatch() noexcept {
    if (!m_uploadBatchActive || m_uploadSubmissionStarted) {
        return;
    }

    m_textureManager->RollbackUpload(
        m_textureUploadCheckpoint
    );

    m_meshManager->RollbackUpload(
        m_meshUploadCheckpoint
    );

    m_descriptorManager->RollbackAllocations(
        m_descriptorUploadCheckpoint
    );

    m_uploadManager->CancelBatch();

    m_uploadBatchActive = false;
}



MappedBufferGpuResource ResourceManager::CreateUploadBuffer(uint64_t size) {
    if (!m_device || !m_heapManager) {
        throw std::logic_error("ResourceManager::CreateUploadBuffer : 尚未初始化");
    }
    if (size == 0) {
        throw std::invalid_argument("ResourceManager::CreateUploadBuffer : Buffer 大小不可為零");
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    MappedBufferGpuResource data{};

    data.allocation = m_heapManager->Allocate(
        desc,
        D3D12_HEAP_TYPE_UPLOAD
    );

    Common::ThrowIfFailed(
        m_device->CreatePlacedResource(
            m_heapManager->GetHeap(data.allocation.heapId),
            data.allocation.offset,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&data.resource)
        ),
        "ResourceManager::CreateUploadBuffer : Placed upload buffer 建立失敗"
    );

    const D3D12_RANGE readRange{ 0, 0 };
    void* mappedData = nullptr;
    Common::ThrowIfFailed(
        data.resource->Map(0, &readRange, &mappedData),
        "ResourceManager::CreateUploadBuffer : Buffer Map 失敗"
    );
    data.cpuAddress = static_cast<unsigned char*>(mappedData);

    return data;
}

void ResourceManager::InitConstantBuffers() {
    static_assert(
        ConstantBufferSize %
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
        "ResourceManager::InitConstantBuffers : CB 大小必須對齊 256 bytes"
        );

    static_assert(
        sizeof(PerFrameConstant) <= ConstantBufferSize,
        "ResourceManager::InitConstantBuffers : PerFrameConstant 超出 CB 容量"
        );

    for (uint32_t frame = 0; frame < FrameCount; ++frame) {
        for (uint32_t slot = 0; slot < MaxCBVsPerFrame; ++slot) {
            m_constantBuffers[frame][slot] =
                CreateUploadBuffer(ConstantBufferSize);
        }
    }
}

void ResourceManager::InitStructuredBuffers() {
    constexpr uint64_t bufferSize = uint64_t{ sizeof(InstanceData) } * MaxInstanceCount;

    for (uint32_t frame = 0; frame < FrameCount; ++frame) {
        m_structuredBuffers[frame] = CreateUploadBuffer(bufferSize);
    }
}

void ResourceManager::UpdatePerFrameCB(const SceneRenderData& data, uint32_t currentFrame) {
    auto& buffer = m_constantBuffers[currentFrame][0];
    if (!buffer.resource || !buffer.cpuAddress) {
        throw std::logic_error("ResourceManager::UpdatePerFrameCB : Constant buffer 尚未初始化");
    }

    std::memcpy(
        buffer.cpuAddress,
        &data.cameraData,
        sizeof(data.cameraData)
    );
}

void ResourceManager::UpdateInstanceSB(const SceneRenderData& data, uint32_t currentFrame) {
    if (data.instanceDatas.size() > MaxInstanceCount) {
        throw std::overflow_error("ResourceManager::UpdateInstanceSB : Instance count 超過最大容量");
    }
    if (data.instanceDatas.empty()) {
        return;
    }

    auto& buffer = m_structuredBuffers[currentFrame];
    if (!buffer.resource || !buffer.cpuAddress) {
        throw std::logic_error("ResourceManager::UpdateInstanceSB : Structured buffer 尚未初始化");
    }

    std::memcpy(
        buffer.cpuAddress,
        data.instanceDatas.data(),
        data.instanceDatas.size() * sizeof(InstanceData)
    );
}



D3D12_GPU_VIRTUAL_ADDRESS ResourceManager::GetConstantBufferAddress(uint32_t currentFrame, uint32_t slot) const {
    if (currentFrame >= FrameCount || slot >= MaxCBVsPerFrame) {
        throw std::out_of_range("ResourceManager::GetConstantBufferAddress : Frame 或 slot 超出範圍");
    }

    const auto& buffer = m_constantBuffers[currentFrame][slot];
    if (!buffer.resource) {
        throw std::logic_error("ResourceManager::GetConstantBufferAddress : Constant buffer 尚未初始化");
    }

    return buffer.resource->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS ResourceManager::GetStructuredBufferAddress(uint32_t currentFrame) const {
    if (currentFrame >= FrameCount) {
        throw std::out_of_range("ResourceManager::GetStructuredBufferAddress : Frame index 超出範圍");
    }

    const auto& buffer = m_structuredBuffers[currentFrame];
    if (!buffer.resource) {
        throw std::logic_error("ResourceManager::GetStructuredBufferAddress : Structured buffer 尚未初始化");
    }

    return buffer.resource->GetGPUVirtualAddress();
}

HeapManager* ResourceManager::GetHeapManager() {
    return m_heapManager.get();
}

UploadManager* ResourceManager::GetUploadManager() {
    return m_uploadManager.get();
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
