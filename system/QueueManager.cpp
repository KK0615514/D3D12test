#include "QueueManager.h"

void QueueManager::Init() {
	InitializeCommandQueue();
	InitializeCommandList();
}

void QueueManager::InitializeCommandQueue() {
    // 建立CommandQueue
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
        "Command Queue建立失敗"
    );

    // 建立Fence for CommandQueue
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueueFence)),
        "Fence for CommandQueue建立失敗"
    );
    m_commandQueueFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void QueueManager::InitializeCommandList() {
    // 建立CommandAllocator&CommandList
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])),
            "Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)),
        "CommandList建立失敗"
    );
}

void QueueManager::BeginCommandRecording(uint32_t currentFrame, ID3D12Resource* pBackBuffer) {
    Common::ThrowIfFailed(
        m_commandAllocator[currentFrame]->Reset(),
        "Allocator重置失敗"
    );
    Common::ThrowIfFailed(
        m_commandList->Reset(m_commandAllocator[currentFrame].Get(), nullptr),
        "CommandList綁定失敗"
    );

    //建立Barrier 更改SwapChian當前Buffer的狀態
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &barrier);
}

void QueueManager::EndAndExecuteCommandRecording(uint32_t currentFrame, ID3D12Resource* pBackBuffer) {
    //修改Barrier 更改SwapChain當前Buffer的狀態
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();
    ID3D12CommandList* cmds[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmds);
}

void QueueManager::WaitFrameReady(uint32_t currentFrame) {
    // 置開頭 等這一幀前一次的送出的fence完成 
    const uint64_t fenceToWait = m_commandQueueFenceValue[currentFrame];
    // 如果 GPU 目前實際完成的進度，還沒趕上這個目標值，CPU 才進入等待
    if (m_commandQueueFence->GetCompletedValue() < fenceToWait)
    {
        m_commandQueueFence->SetEventOnCompletion(fenceToWait, m_commandQueueFenceEvent);
        WaitForSingleObject(m_commandQueueFenceEvent, INFINITE);
    }
}

void QueueManager::SignalCurrentFrame(uint32_t currentFrame) {
    // 置結尾 一般render用fence函式 後+保證連續遞增規律
    const uint64_t value = m_currentFenceValue++;
    // 送出fence  下幀在等
    Common::ThrowIfFailed(
        m_commandQueue->Signal(m_commandQueueFence.Get(), value),
        "幀末的fence失效"
    );
    m_commandQueueFenceValue[currentFrame] = value;
}

void QueueManager::FlushCommandQueue() {
    const uint64_t value = m_currentFenceValue++;
    Common::ThrowIfFailed(
        m_commandQueue->Signal(m_commandQueueFence.Get(), value),
        "Flush 失敗"
    );

    if (m_commandQueueFence->GetCompletedValue() < value) {
        Common::ThrowIfFailed(
            m_commandQueueFence->SetEventOnCompletion(value, m_commandQueueFenceEvent),
            "Fence event設定失敗"
        );
        WaitForSingleObject(m_commandQueueFenceEvent, INFINITE);
    }
}
    