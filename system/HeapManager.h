#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

struct HeapAllocation {
    uint32_t heapId = UINT32_MAX;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct HeapBlock {
    ComPtr<ID3D12Heap> heap;

    D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

    uint64_t capacity = 0;
    uint64_t cursor = 0;
};



class HeapManager {
public:
    HeapManager(ID3D12Device4* device): m_device(device) {}
    ~HeapManager() = default;

    HeapManager (const HeapManager&) = delete;
    HeapManager (HeapManager&&) = delete;
    HeapManager& operator=(const HeapManager&) = delete;
    HeapManager& operator=(HeapManager&&) = delete;

    void Init(      // 預設 64MB / 16MB
        uint64_t defaultBlockSize = 64ull * 1024 * 1024,
        uint64_t uploadBlockSize = 16ull * 1024 * 1024);

    HeapAllocation Allocate(const D3D12_RESOURCE_DESC& desc, D3D12_HEAP_TYPE type);

    ID3D12Heap* GetHeap(uint32_t heapId) const;

private:
    ComPtr<ID3D12Device4> m_device;

    // 所有配置池的實體 heap，heapId 是這個集合的索引。
    std::vector<HeapBlock> m_heaps;

    uint64_t m_defaultBlockSize = 0;
    uint64_t m_uploadBlockSize = 0;

    D3D12_RESOURCE_HEAP_TIER m_heapTier = D3D12_RESOURCE_HEAP_TIER_1;

    static uint64_t AlignUp(uint64_t value, uint64_t alignment);
    D3D12_HEAP_FLAGS SelectHeapFlags(const D3D12_RESOURCE_DESC& desc, D3D12_HEAP_TYPE type) const;
    uint32_t CreateHeap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags, uint64_t minimumSize);
};