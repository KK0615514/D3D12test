#include "system/UploadManager.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#include "system/CommonUtils.h"

void UploadManager::Init(uint64_t blockSize) {
    if (!m_device || !m_heapManager || blockSize == 0) {
        throw std::invalid_argument("UploadManager::Init : 無效 Device、HeapManager 或 BlockSize");
    }
    if (m_blockSize != 0) {
        throw std::logic_error("UploadManager::Init : 已經初始化");
    }

    const uint64_t alignedSize = AlignUp(
        blockSize,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    );

    if (alignedSize > std::numeric_limits<size_t>::max()) {
        throw std::overflow_error("UploadManager::Init : BlockSize 超出 CPU 可定址範圍");
    }

    m_blockSize = alignedSize;
}

// 左邊對齊右邊倍數
uint64_t UploadManager::AlignUp(uint64_t value,uint64_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw std::invalid_argument("UploadManager::AlignUp : Alignment 必須是非零的 2 次方");
    }

    const uint64_t padding = alignment - 1;
    if (value > std::numeric_limits<uint64_t>::max() - padding) {
        throw std::overflow_error("UploadManager::AlignUp : 對齊計算溢位");
    }

    return (value + padding) & ~padding;
}

void UploadManager::BeginBatch() {
    if (m_blockSize == 0) {
        throw std::logic_error("UploadManager::BeginBatch : 尚未初始化");
    }
    if (m_batchActive) {
        throw std::logic_error("UploadManager::BeginBatch : 已有進行中的批次");
    }
    for (const auto& block : m_uploadBuffers) {
        if (block.hasPendingSubmission) {
            throw std::logic_error("UploadManager::BeginBatch : 存在未處理的上傳切片");
        }
    }

    m_batchActive = true;
}

void UploadManager::CancelBatch() {
    if (!m_batchActive) {
        return;
    }
    for (auto& block : m_uploadBuffers) {
        if (!block.hasPendingSubmission) {
            continue;
        }

        // 這些 blocks 尚未提交給 GPU。
        block.cursor = 0;
        block.hasPendingSubmission = false;
    }

    m_batchActive = false;
}

void UploadManager::CreateBuffer(uint64_t minimumSize) {
    const uint64_t capacity = AlignUp(
        std::max(m_blockSize, minimumSize),
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    );

    if (capacity > std::numeric_limits<size_t>::max()) {
        throw std::overflow_error("UploadManager::CreateBuffer : Buffer 超出 CPU 可定址範圍");
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacity;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    UploadBufferBlock block{};
    block.capacity = capacity;

    block.allocation = m_heapManager->Allocate(
        desc,
        D3D12_HEAP_TYPE_UPLOAD
    );

    Common::ThrowIfFailed(
        m_device->CreatePlacedResource(
            m_heapManager->GetHeap(block.allocation.heapId),
            block.allocation.offset,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&block.resource)
        ),
        "UploadManager::CreateBuffer : Placed upload buffer 建立失敗"
    );

    const D3D12_RANGE readRange{ 0, 0 };
    void* mappedData = nullptr;
    Common::ThrowIfFailed(
        block.resource->Map(0, &readRange, &mappedData),
        "UploadManager::CreateBuffer : Upload buffer Map 失敗"
    );

    block.cpuAddress = static_cast<unsigned char*>(mappedData);

    m_uploadBuffers.push_back(std::move(block));
}

// 分配buffer內的可用資源offset
UploadSlice UploadManager::Allocate(uint64_t size,uint64_t alignment) {
    if (m_blockSize == 0) {
        throw std::logic_error("UploadManager::Allocate : 尚未初始化");
    }
    if (!m_batchActive) {
        throw std::logic_error("UploadManager::Allocate : 尚未開始上傳批次");
    }
    if (size == 0) {
        throw std::invalid_argument("UploadManager::Allocate : Size 不可為零");
    }
    if (size > std::numeric_limits<size_t>::max()) {
        throw std::overflow_error("UploadManager::Allocate : Size 超出 CPU 可定址範圍");
    }

    // 同時檢查 alignment 是否有效。
    AlignUp(0, alignment);

    for (auto& block : m_uploadBuffers) {
        // GPU 還在讀取的 block 不配置新切片。
        if (block.copyFenceValue != 0) {
            continue;
        }

        const uint64_t offset = AlignUp(
            block.cursor,
            alignment
        );

        if (offset > block.capacity || size > block.capacity - offset) {
            continue;
        }

        block.cursor = offset + size;
        block.hasPendingSubmission = true;

        return UploadSlice{
            block.resource.Get(),
            offset,
            size,
            block.cpuAddress + static_cast<size_t>(offset)
        };
    }

    // 現有 block 沒有足夠空間，建立另一個。
    CreateBuffer(size);

    auto& block = m_uploadBuffers.back();
    block.cursor = size;
    block.hasPendingSubmission = true;

    return UploadSlice{
        block.resource.Get(),
        0,
        size,
        block.cpuAddress
    };
}

// 登記該次上傳進upload的fence值
void UploadManager::MarkSubmitted(uint64_t copyFenceValue) {
    if (!m_batchActive) {
        throw std::logic_error("UploadManager::MarkSubmitted : 沒有進行中的批次");
    }
    if (copyFenceValue == 0 || copyFenceValue == UINT64_MAX) {
        throw std::invalid_argument("UploadManager::MarkSubmitted : 無效 Copy fence value");
    }

    for (auto& block : m_uploadBuffers) {
        if (!block.hasPendingSubmission) {
            continue;
        }

        block.hasPendingSubmission = false;
        block.copyFenceValue = copyFenceValue;
    }

    m_batchActive = false;
}

// 檢查整體進度 全部完成就reset cursor
void UploadManager::Collect(uint64_t completedCopyFenceValue) {
    if (completedCopyFenceValue == UINT64_MAX) {
        throw std::runtime_error("UploadManager::Collect : 裝置已移除，不能視為上傳完成");
    }

    for (auto& block : m_uploadBuffers) {
        if (block.hasPendingSubmission ||
            block.copyFenceValue == 0 ||
            block.copyFenceValue > completedCopyFenceValue) {
            continue;
        }

        // 所有切片都已完成 copy，可以整個 block 從頭配置。
        block.cursor = 0;
        block.copyFenceValue = 0;
    }
}