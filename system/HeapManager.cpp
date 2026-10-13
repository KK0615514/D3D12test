#include "system/HeapManager.h"

#include <cstddef>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <limits>

#include "system/CommonUtils.h"

void HeapManager::Init(uint64_t defaultBlockSize, uint64_t uploadBlockSize) {
    if (!m_device || defaultBlockSize == 0 || uploadBlockSize == 0){
        throw std::invalid_argument("HeapManager::Init : 無效 Device 或 BlockSize");
    }
    if (m_defaultBlockSize != 0) {
        throw std::logic_error("HeapManager::Init : 已經初始化");
    }

    const uint64_t alignedDefaultSize = AlignUp(
        defaultBlockSize,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    );
    const uint64_t alignedUploadSize = AlignUp(
        uploadBlockSize,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    );

    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};

    Common::ThrowIfFailed(
        m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS,
            &options,
            sizeof(options)
        ),
        "HeapManager::Init : Heap Tier 查詢失敗"
    );

    m_heapTier = options.ResourceHeapTier;
    m_defaultBlockSize = alignedDefaultSize;
    m_uploadBlockSize = alignedUploadSize;
}

uint64_t HeapManager::AlignUp(uint64_t value, uint64_t alignment){
    if (alignment == 0 || (alignment & (alignment - 1)) != 0){
        throw std::invalid_argument("HeapManager::AlignUp : Alignment 必須是非零的 2 次方");
    }

    const uint64_t padding = alignment - 1;
    if (value > std::numeric_limits<uint64_t>::max() - padding) {
        throw std::overflow_error("HeapManager::AlignUp : 對齊計算溢位");
    }

    return (value + padding) & ~padding;
}



HeapAllocation HeapManager::Allocate(const D3D12_RESOURCE_DESC& desc,D3D12_HEAP_TYPE type)
{
    if (m_defaultBlockSize == 0) {
        throw std::logic_error("HeapManager::Allocate : 尚未初始化");
    }

    const bool isBuffer =
        desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
    const bool isTexture =
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    if ((!isBuffer && !isTexture) || desc.Width == 0) {
        throw std::invalid_argument("HeapManager::Allocate : 無效 Resource 描述");
    }

    // 初版使用標準非 MSAA heap。
    if (desc.SampleDesc.Count != 1 || desc.SampleDesc.Quality != 0) {
        throw std::invalid_argument("HeapManager::Allocate : 目前不支援 MSAA");
    }

    const D3D12_HEAP_FLAGS flags = SelectHeapFlags(desc, type);

    const D3D12_RESOURCE_ALLOCATION_INFO info =
        m_device->GetResourceAllocationInfo(
            0,
            1,
            &desc
        );

    if (info.SizeInBytes == UINT64_MAX || info.SizeInBytes == 0 || info.Alignment == 0){
        throw std::invalid_argument("HeapManager::Allocate : 無法取得有效配置資訊");
    }

    if (info.Alignment > D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) {
        throw std::invalid_argument("HeapManager::Allocate : 不支援此 Resource 對齊需求");
    }

    for (std::size_t i = 0; i < m_heaps.size(); ++i) {
        auto& block = m_heaps[i];

        // 只在相容的池中配置。
        if (block.type != type || block.flags != flags) {
            continue;
        }

        const uint64_t offset = AlignUp(block.cursor, info.Alignment);
        if (offset <= block.capacity && info.SizeInBytes <= block.capacity - offset){
            block.cursor = offset + info.SizeInBytes;

            return HeapAllocation{
                static_cast<uint32_t>(i),
                offset,
                info.SizeInBytes
            };
        }
    }

    const uint32_t heapId = CreateHeap(type, flags, info.SizeInBytes);
    auto& block = m_heaps[heapId];
    block.cursor = info.SizeInBytes;

    return HeapAllocation{
        heapId,
        0,
        info.SizeInBytes
    };
}

D3D12_HEAP_FLAGS HeapManager::SelectHeapFlags(const D3D12_RESOURCE_DESC& desc, D3D12_HEAP_TYPE type) const {
    if (type == D3D12_HEAP_TYPE_UPLOAD) {
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
            throw std::invalid_argument("HeapManager::SelectHeapFlags : UPLOAD 僅支援 Buffer");
        }

        // 初版 UPLOAD 用於普通上傳、CB／SB buffer。
        if (desc.Flags != D3D12_RESOURCE_FLAG_NONE) {
            throw std::invalid_argument("HeapManager::SelectHeapFlags : UPLOAD 不支援此 Resource Flags");
        }

        return D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    }

    if (type != D3D12_HEAP_TYPE_DEFAULT) {
        throw std::invalid_argument("HeapManager::SelectHeapFlags : 目前僅支援 DEFAULT 與 UPLOAD");
    }

    if (m_heapTier >= D3D12_RESOURCE_HEAP_TIER_2) {
        return D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
    }

    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        return D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    }

    if (desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
        return D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
    }

    return D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
}

uint32_t HeapManager::CreateHeap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags, uint64_t minimumSize)
{
    if (m_heaps.size() >= UINT32_MAX) {
        throw std::overflow_error("HeapManager::CreateHeap : Heap 數量超出上限");
    }

    const uint64_t blockSize =
        type == D3D12_HEAP_TYPE_UPLOAD
        ? m_uploadBlockSize
        : m_defaultBlockSize;

    const uint64_t capacity = AlignUp(
        std::max(blockSize, minimumSize),
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    );

    D3D12_HEAP_DESC desc{};
    desc.SizeInBytes = capacity;
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Properties.Type = type;
    desc.Properties.CreationNodeMask = 1;
    desc.Properties.VisibleNodeMask = 1;
    desc.Flags = flags;

    HeapBlock block{};
    block.type = type;
    block.flags = flags;
    block.capacity = capacity;

    Common::ThrowIfFailed(
        m_device->CreateHeap(
            &desc,
            IID_PPV_ARGS(&block.heap)
        ),
        "HeapManager::CreateHeap : Heap 建立失敗"
    );

    const uint32_t heapId =
        static_cast<uint32_t>(m_heaps.size());

    m_heaps.push_back(std::move(block));

    return heapId;
}



ID3D12Heap* HeapManager::GetHeap(uint32_t heapId) const {
    if (heapId >= m_heaps.size() ||
        !m_heaps[heapId].heap)
    {
        throw std::out_of_range(
            "HeapManager::GetHeap : 無效 HeapId"
        );
    }

    return m_heaps[heapId].heap.Get();
}