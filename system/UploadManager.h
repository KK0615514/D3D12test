#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

#include "system/HeapManager.h"

using Microsoft::WRL::ComPtr;

// 借用 UploadManager 持有的資源。
struct UploadSlice {
    ID3D12Resource* resource = nullptr;

    uint64_t offset = 0;                    // offset 是 buffer 內偏移，不是 heap 內偏移。
    uint64_t size = 0;

    unsigned char* cpuAddress = nullptr;    // cpuAddress 已經包含這個 offset。
};

struct UploadBufferBlock {
    ComPtr<ID3D12Resource> resource;
    HeapAllocation allocation{};

    unsigned char* cpuAddress = nullptr;

    uint64_t capacity = 0;
    uint64_t cursor = 0;

    bool hasPendingSubmission = false;                     // 此 block 是否含有尚未提交的切片。
    uint64_t copyFenceValue = 0;                // 已提交批次的 Copy fence；0 表示沒有等待中的批次。
};



class UploadManager {
public:
    UploadManager(ID3D12Device4* device, HeapManager* heapManager)
        : m_device(device), m_heapManager(heapManager) {}
    ~UploadManager() = default;

    UploadManager(const UploadManager&) = delete;
    UploadManager(UploadManager&&) = delete;
    UploadManager& operator=(const UploadManager&) = delete;
    UploadManager& operator=(UploadManager&&) = delete;

    void Init( uint64_t blockSize = 16ull * 1024 * 1024);

    UploadSlice Allocate(uint64_t size, uint64_t alignment = 4);

 
    void BeginBatch();              // 開始一個新的 CPU 上傳準備批次。
    void CancelBatch();             // 撤回尚未提交的切片，使它們所在的 buffers 可以重新配置。


    void MarkSubmitted(uint64_t copyFenceValue);        // 將目前錄製批次用到的 blocks 標記為已提交。

    void Collect(uint64_t completedCopyFenceValue);     // 非阻塞：只重設 Copy fence 已完成的 blocks。

private:
    ComPtr<ID3D12Device4> m_device;
    HeapManager* m_heapManager = nullptr;

    uint64_t m_blockSize = 0;
    bool m_batchActive = false;

    std::vector<UploadBufferBlock> m_uploadBuffers;

    static uint64_t AlignUp(uint64_t value, uint64_t alignment);
    void CreateBuffer(uint64_t minimumSize);
};