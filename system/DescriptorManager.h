#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cassert>
#include <cstdint>

struct DescriptorSlot {
    uint32_t index = UINT32_MAX;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
};



class DescriptorManager {
public:
    DescriptorManager(ID3D12Device4* device) : m_device(device) {}
    ~DescriptorManager() = default;

    void Init(uint32_t capacity = 256);

    DescriptorSlot Allocate();

    uint32_t GetAllocationCount() const;
    void RollbackAllocations(uint32_t count);    // 僅用於撤回尚未提交、尚未對外發布的新增 descriptors。

    ID3D12DescriptorHeap* GetShaderHeap() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetTableStart() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Device4>           m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_shaderHeap;

    uint32_t    m_capacity          = 0;
    uint32_t    m_cursor            = 0;
    UINT        m_descriptorSize    = 0;
};