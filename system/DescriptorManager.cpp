#include "system/DescriptorManager.h"

#include <stdexcept>

#include "system/CommonUtils.h"

void DescriptorManager::Init(uint32_t capacity) {
    // 檢查資料合不合規
    if (!m_device || capacity == 0) {
        throw std::invalid_argument("DescriptorManager::Init : 無效參數");
    }
    if (m_shaderHeap) {
        throw std::logic_error("DescriptorManager::Init : 已經初始化");
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = capacity;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Common::ThrowIfFailed(
        m_device->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&m_shaderHeap)),
        "DescriptorManager::Init : Descriptor heap 建立失敗"
    );

    m_capacity       = capacity;
    m_cursor         = 0;
    m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

DescriptorSlot DescriptorManager::Allocate() {
    // 檢查容量以及初始化與否
    if (!m_shaderHeap) {
        throw std::logic_error("DescriptorManager::Allocate : 尚未初始化");
    }
    if (m_cursor >= m_capacity) {
        throw std::overflow_error("DescriptorManager::Allocate : Descriptor heap 容量已滿");
    }

    DescriptorSlot slot{};
    slot.index = m_cursor;

    slot.cpu = m_shaderHeap->GetCPUDescriptorHandleForHeapStart();
    slot.cpu.ptr += SIZE_T(slot.index) * m_descriptorSize;

    slot.gpu = m_shaderHeap->GetGPUDescriptorHandleForHeapStart();
    slot.gpu.ptr += UINT64(slot.index) * m_descriptorSize;

    ++m_cursor;
    return slot;
}

ID3D12DescriptorHeap* DescriptorManager::GetShaderHeap() const {
    return m_shaderHeap.Get();
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorManager::GetTableStart() const {
    return m_shaderHeap->GetGPUDescriptorHandleForHeapStart();
}