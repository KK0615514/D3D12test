#include "QueueManager.h"

void QueueManager::Init() {
	InitializeCommandQueue();
	InitializeCommandList();
}

void QueueManager::InitializeCommandQueue() {
    m_commandQueues.resize(3);

    // 建立DirectQueue
    D3D12_COMMAND_QUEUE_DESC directQueueDesc{};
    directQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    directQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&directQueueDesc, IID_PPV_ARGS(&m_commandQueues[0].commandQueue)),
        "DirectQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[0].fence)),
        "Fence for DirectQueue建立失敗"
    );
    m_commandQueues[0].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // 建立CopyQueue
    D3D12_COMMAND_QUEUE_DESC copyQueueDesc{};
    copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_commandQueues[1].commandQueue)),
        "CopyQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[1].fence)),
        "Fence for CopyQueue建立失敗"
    );
    m_commandQueues[1].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // 建立ComputeQueue
    D3D12_COMMAND_QUEUE_DESC computeQueueDesc{};
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_commandQueues[2].commandQueue)),
        "ComputeQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[2].fence)),
        "Fence for ComputeQueue建立失敗"
    );
    m_commandQueues[2].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void QueueManager::InitializeCommandList() {
    // 建立Allocator&List for Direct
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandQueues[0].commandAllocators[i])),
            "Direct Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[0].commandList)),
        "Direct CommandList建立失敗"
    );

    // 建立Allocator&List for Copy
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_commandQueues[1].commandAllocators[i])),
            "Direct Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[1].commandList)),
        "Direct CommandList建立失敗"
    );

    // 建立Allocator&List for Compute
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_commandQueues[2].commandAllocators[i])),
            "Direct Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[2].commandList)),
        "Direct CommandList建立失敗"
    );
}

void QueueManager::BeginCommandRecording(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    Common::ThrowIfFailed(
        currentQueue.commandAllocators[currentFrame]->Reset(),
        "Allocator重置失敗"
    );
    Common::ThrowIfFailed(
        currentQueue.commandList->Reset(currentQueue.commandAllocators[currentFrame].Get(), nullptr),
        "CommandList綁定失敗"
    );
}

void QueueManager::EndAndExecuteCommandRecording(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    currentQueue.commandList->Close();
    ID3D12CommandList* cmds[] = { currentQueue.commandList.Get() };
    currentQueue.commandQueue->ExecuteCommandLists(1, cmds);
}

void QueueManager::WaitFrameReady(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    // 置開頭 等這一幀前一次的送出的fence完成 
    const uint64_t fenceToWait = currentQueue.fenceValues[currentFrame];
    // 如果 GPU 目前實際完成的進度，還沒趕上這個目標值，CPU 才進入等待
    if (currentQueue.fence->GetCompletedValue() < fenceToWait)
    {
        currentQueue.fence->SetEventOnCompletion(fenceToWait, currentQueue.fenceEvent);
        WaitForSingleObject(currentQueue.fenceEvent, INFINITE);
    }
}

void QueueManager::SignalCurrentFrame(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    // 置結尾 一般render用fence函式 後+保證連續遞增規律
    const uint64_t value = currentQueue.currentFenceValue++;
    // 送出fence  下幀在等
    Common::ThrowIfFailed(
        currentQueue.commandQueue->Signal(currentQueue.fence.Get(), value),
        "幀末的fence失效"
    );
    currentQueue.fenceValues[currentFrame] = value;
}

void QueueManager::FlushCommandQueue(QueueType queue) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    const uint64_t value = currentQueue.currentFenceValue++;
    Common::ThrowIfFailed(
        currentQueue.commandQueue->Signal(currentQueue.fence.Get(), value),
        "Flush 失敗"
    );
    if (currentQueue.fence->GetCompletedValue() < value) {
        Common::ThrowIfFailed(
            currentQueue.fence->SetEventOnCompletion(value, currentQueue.fenceEvent),
            "Fence event設定失敗"
        );
        WaitForSingleObject(currentQueue.fenceEvent, INFINITE);
    }
}
    
ID3D12CommandQueue* QueueManager::GetQueue(QueueType queue) {
    return m_commandQueues[static_cast<size_t>(queue)].commandQueue.Get();
}

ID3D12GraphicsCommandList1* QueueManager::GetList(QueueType queue) {
    return m_commandQueues[static_cast<size_t>(queue)].commandList.Get();
}