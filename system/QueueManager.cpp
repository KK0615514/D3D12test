#include "QueueManager.h"

#include <stdexcept>
#include <string>
#include <utility>

CommandQueueContext::~CommandQueueContext() {
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
}
    
void QueueManager::Init() {
    InitCommandQueue();
    InitCommandList();
}

void QueueManager::InitCommandQueue() {
    // 建立DirectQueue
    D3D12_COMMAND_QUEUE_DESC directQueueDesc{};
    directQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    directQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&directQueueDesc, IID_PPV_ARGS(&m_commandQueues[0].commandQueue)),
        "QueueManager::InitCommandQueue : DirectQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[0].fence)),
        "QueueManager::InitCommandQueue : Fence for DirectQueue建立失敗"
    );
    m_commandQueues[0].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_commandQueues[0].fenceEvent) 
        throw std::runtime_error("QueueManager::InitCommandQueue : Fence event建立失敗");
    
    // 建立CopyQueue
    D3D12_COMMAND_QUEUE_DESC copyQueueDesc{};
    copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_commandQueues[1].commandQueue)),
        "QueueManager::InitCommandQueue : CopyQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[1].fence)),
        "QueueManager::InitCommandQueue : Fence for CopyQueue建立失敗"
    );
    m_commandQueues[1].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_commandQueues[1].fenceEvent) 
        throw std::runtime_error("QueueManager::InitCommandQueue : Fence event建立失敗");

    // 建立ComputeQueue
    D3D12_COMMAND_QUEUE_DESC computeQueueDesc{};
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    Common::ThrowIfFailed(
        m_device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_commandQueues[2].commandQueue)),
        "QueueManager::InitCommandQueue : ComputeQueue建立失敗"
    );
    Common::ThrowIfFailed(
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[2].fence)),
        "QueueManager::InitCommandQueue : Fence for ComputeQueue建立失敗"
    );
    m_commandQueues[2].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_commandQueues[2].fenceEvent) 
        throw std::runtime_error("QueueManager::InitCommandQueue : Fence event建立失敗");
}

void QueueManager::InitCommandList() {
    // 建立Allocator&List for Direct
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandQueues[0].commandAllocators[i])),
            "QueueManager::InitCommandList : Direct Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[0].commandList)),
        "QueueManager::InitCommandList : Direct CommandList建立失敗"
    );

    // 建立Allocator&List for Copy
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_commandQueues[1].commandAllocators[i])),
            "QueueManager::InitCommandList : Copy Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[1].commandList)),
        "QueueManager::InitCommandList : Copy CommandList建立失敗"
    );

    // 建立Allocator&List for Compute
    for (uint32_t i = 0; i < FrameCount;i++)
    {
        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_commandQueues[2].commandAllocators[i])),
            "QueueManager::InitCommandList : Compute Allocator建立失敗"
        );
    }
    Common::ThrowIfFailed(
        m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandQueues[2].commandList)),
        "QueueManager::InitCommandList : Compute CommandList建立失敗"
    );
}



void QueueManager::BeginCommandRecording(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    Common::ThrowIfFailed(
        currentQueue.commandAllocators[currentFrame]->Reset(),
        "QueueManager::BeginCommandRecording : Allocator重置失敗"
    );  
    Common::ThrowIfFailed(
        currentQueue.commandList->Reset(currentQueue.commandAllocators[currentFrame].Get(), nullptr),
        "QueueManager::BeginCommandRecording : CommandList綁定失敗"
    );
}

void QueueManager::EndAndExecuteCommandRecording(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    Common::ThrowIfFailed(
        currentQueue.commandList->Close(),
        "QueueManager::EndAndExecuteCommandRecording : CommandList close 失敗"
    );
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
        Common::ThrowIfFailed(
            currentQueue.fence->SetEventOnCompletion(fenceToWait, currentQueue.fenceEvent),
            "QueueManager::WaitFrameReady : SetEventOnCompletion 失敗"
        );
        const DWORD result = WaitForSingleObject(currentQueue.fenceEvent, INFINITE);
        if (result == WAIT_FAILED) {
            const DWORD error = GetLastError();
            throw std::runtime_error(
                std::string("QueueManager::WaitFrameReady : Fence 等待失敗，Win32 error=") +
                std::to_string(error)
            );
        }
        if (result != WAIT_OBJECT_0) {
            throw std::runtime_error(
                std::string("QueueManager::WaitFrameReady : 非預期的等待結果，result=") +
                std::to_string(result)
            );
        }
    }
}

void QueueManager::SignalCurrentFrame(QueueType queue, uint32_t currentFrame) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    // 置結尾 一般render用fence函式 後+保證連續遞增規律
    const uint64_t value = currentQueue.currentFenceValue++;
    // 送出fence  下幀在等
    Common::ThrowIfFailed(
        currentQueue.commandQueue->Signal(currentQueue.fence.Get(), value),
        "QueueManager::SignalCurrentFrame : 幀末的fence失效"
    );
    currentQueue.fenceValues[currentFrame] = value;
}

void QueueManager::FlushCommandQueue(QueueType queue) {
    CommandQueueContext& currentQueue = m_commandQueues[static_cast<size_t>(queue)];

    const uint64_t value = currentQueue.currentFenceValue++;
    Common::ThrowIfFailed(
        currentQueue.commandQueue->Signal(currentQueue.fence.Get(), value),
        "QueueManager::FlushCommandQueue : Flush 失敗"
    );
    if (currentQueue.fence->GetCompletedValue() < value) {
        Common::ThrowIfFailed(
            currentQueue.fence->SetEventOnCompletion(value, currentQueue.fenceEvent),
            "QueueManager::FlushCommandQueue : Fence event設定失敗"
        );
        const DWORD result = WaitForSingleObject(currentQueue.fenceEvent, INFINITE);
        if (result == WAIT_FAILED) {
            const DWORD error = GetLastError();

            throw std::runtime_error(
                std::string("QueueManager::FlushCommandQueue : Fence 等待失敗，Win32 error=") +
                std::to_string(error)
            );
        }
        if (result != WAIT_OBJECT_0) {
            throw std::runtime_error(
                std::string("QueueManager::FlushCommandQueue : 非預期的等待結果，result=") +
                std::to_string(result)
            );
        }
    }
}
    

// 指定的queue送出signal 完成後fence改變為value
uint64_t QueueManager::Signal(QueueType queue){
    auto& context = m_commandQueues.at(static_cast<size_t>(queue));
    const uint64_t value = context.currentFenceValue++;

    Common::ThrowIfFailed(
        context.commandQueue->Signal(context.fence.Get(), value),
        "QueueManager::Signal : Queue signal 失敗"
    );
    return value;
}
// 接受fencvalue 第一個queue等待第二個queue的fence的值到fencevalue
void QueueManager::QueueWait( QueueType waitingQueue,QueueType sourceQueue,uint64_t fenceValue){
    auto& waiting = m_commandQueues.at(static_cast<size_t>(waitingQueue));
    auto& source = m_commandQueues.at(static_cast<size_t>(sourceQueue));

    Common::ThrowIfFailed(
        waiting.commandQueue->Wait(source.fence.Get(), fenceValue),
        "QueueManager::QueueWait : Queue wait 失敗"
    );
}



ID3D12GraphicsCommandList1* QueueManager::BeginUploadRecording(QueueType queue) {
    if (queue != QueueType::Copy && queue != QueueType::Direct) {
        throw std::invalid_argument("QueueManager::BeginUploadRecording : 僅支援 Copy 或 Direct");
    }

    auto& context = m_commandQueues[static_cast<size_t>(queue)];
    if (context.activeUploadContext != UINT32_MAX) {
        throw std::logic_error("QueueManager::BeginUploadRecording : 已有進行中的錄製");
    }

    const uint64_t completed = GetCompletedFenceValue(queue);

    uint32_t selected = UINT32_MAX;

    for (std::size_t i = 0; i < context.uploadContexts.size(); ++i) {
        const auto& item = context.uploadContexts[i];

        if (item.retireFenceValue != UINT64_MAX && item.retireFenceValue <= completed) {
            selected = static_cast<uint32_t>(i);
            break;
        }
    }

    const D3D12_COMMAND_LIST_TYPE type =
        queue == QueueType::Copy
        ? D3D12_COMMAND_LIST_TYPE_COPY
        : D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (selected == UINT32_MAX) {
        if (context.uploadContexts.size() >= UINT32_MAX) {
            throw std::overflow_error("QueueManager::BeginUploadRecording : Context 數量超出上限");
        }

        UploadCommandContext item{};

        Common::ThrowIfFailed(
            m_device->CreateCommandAllocator(
                type,
                IID_PPV_ARGS(&item.allocator)
            ),
            "QueueManager::BeginUploadRecording : Upload allocator 建立失敗"
        );

        selected = static_cast<uint32_t>(context.uploadContexts.size());

        context.uploadContexts.push_back(std::move(item));
    }

    // 後續準備失敗時，呼叫端可以取消這個 context。
    context.activeUploadContext = selected;

    auto& item = context.uploadContexts[selected];

    // 被取消的 list 會釋放；下次使用時重新建立。
    if (!item.commandList) {
        Common::ThrowIfFailed(
            m_device->CreateCommandList1(
                0,
                type,
                D3D12_COMMAND_LIST_FLAG_NONE,
                IID_PPV_ARGS(&item.commandList)
            ),
            "QueueManager::BeginUploadRecording : Upload command list 建立失敗"
        );
    }

    Common::ThrowIfFailed(
        item.allocator->Reset(),
        "QueueManager::BeginUploadRecording : Upload allocator 重設失敗"
    );

    Common::ThrowIfFailed(
        item.commandList->Reset(item.allocator.Get(), nullptr),
        "QueueManager::BeginUploadRecording : Upload command list 重設失敗"
    );

    item.retireFenceValue = 0;
    item.isClosed = false;

    return item.commandList.Get();
}

void QueueManager::CloseUploadRecording(QueueType queue) {
    if (queue != QueueType::Copy && queue != QueueType::Direct) {
        throw std::invalid_argument("QueueManager::CloseUploadRecording : 僅支援 Copy 或 Direct");
    }

    auto& context = m_commandQueues[static_cast<size_t>(queue)];
    if (context.activeUploadContext == UINT32_MAX) {
        throw std::logic_error("QueueManager::CloseUploadRecording : 沒有進行中的錄製");
    }

    auto& item = context.uploadContexts[context.activeUploadContext];
    if (item.isClosed) {
        throw std::logic_error("QueueManager::CloseUploadRecording : Command list 已 Close");
    }

    Common::ThrowIfFailed(
        item.commandList->Close(),
        "QueueManager::CloseUploadRecording : Command list Close 失敗"
    );

    item.isClosed = true;
}

uint64_t QueueManager::ExecuteUploadRecording(QueueType queue) {
    if (queue != QueueType::Copy && queue != QueueType::Direct) {
        throw std::invalid_argument("QueueManager::ExecuteUploadRecording : 僅支援 Copy 或 Direct");
    }

    auto& context = m_commandQueues[static_cast<size_t>(queue)];
    if (context.activeUploadContext == UINT32_MAX) {
        throw std::logic_error("QueueManager::ExecuteUploadRecording : 沒有待提交的錄製");
    }

    auto& item = context.uploadContexts[context.activeUploadContext];
    if (!item.isClosed) {
        throw std::logic_error("QueueManager::ExecuteUploadRecording : Command list 尚未 Close");
    }

    ID3D12CommandList* lists[] = {
        item.commandList.Get()
    };

    // 一旦送出，不能再把它當成未提交批次取消。
    // Signal 失敗時，保留禁止重用的標記。
    item.retireFenceValue = UINT64_MAX;
    context.activeUploadContext = UINT32_MAX;

    context.commandQueue->ExecuteCommandLists(1, lists);

    const uint64_t fenceValue = Signal(queue);

    item.retireFenceValue = fenceValue;

    return fenceValue;
}

void QueueManager::CancelUploadRecording(QueueType queue){
    const size_t index = static_cast<size_t>(queue);
    if (index >= m_commandQueues.size()) {
        return;
    }

    auto& context = m_commandQueues[index];
    if (context.activeUploadContext == UINT32_MAX) {
        return;
    }

    auto& item = context.uploadContexts[context.activeUploadContext];

    // 尚未 Execute，可以丟棄這份 list。
    // allocator 留到下一次使用時再 Reset。
    item.commandList.Reset();
    item.isClosed = false;
    item.retireFenceValue = 0;

    context.activeUploadContext = UINT32_MAX;
}



ID3D12CommandQueue* QueueManager::GetQueue(QueueType queue) const {
    return m_commandQueues.at(static_cast<size_t>(queue)).commandQueue.Get();
}

ID3D12GraphicsCommandList1* QueueManager::GetList(QueueType queue) const {
    return m_commandQueues.at(static_cast<size_t>(queue)).commandList.Get();
} 

uint64_t QueueManager::GetCompletedFenceValue(QueueType queue) const {
    const size_t index = static_cast<size_t>(queue);
    if (index >= m_commandQueues.size()) {
        throw std::out_of_range("QueueManager::GetCompletedFenceValue : 無效 QueueType");
    }

    const auto& context = m_commandQueues[index];
    if (!context.fence) {
        throw std::logic_error("QueueManager::GetCompletedFenceValue : Fence 不存在");
    }

    const uint64_t completed = context.fence->GetCompletedValue();
    if (completed == UINT64_MAX) {
        throw std::runtime_error("QueueManager::GetCompletedFenceValue : 裝置已移除");
    }

    return completed;
}