#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <vector>

#include "CommonUtils.h"

using Microsoft::WRL::ComPtr;

enum class QueueType: size_t{
	Direct	= 0,
	Copy	= 1,
	Compute = 2
};

struct CommandQueueContext
{
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	// Queue 與命令列表核心
	ComPtr<ID3D12CommandQueue>          commandQueue{};
	ComPtr<ID3D12CommandAllocator>      commandAllocators[FrameCount]{};	//配合FrameCount數量設置Allocator			未來擴增為多執行緒版本	目前為單組allocator配合frameCount多緩衝
	ComPtr<ID3D12GraphicsCommandList1>  commandList{};						//配合CreateCommandList1解藕allocator		未來擴增	cpp部分ppcommandlist修改為size

	// Fence
	ComPtr<ID3D12Fence>                 fence{};							// 基本無印 多顯卡用1
	uint64_t                            fenceValues[FrameCount]{ 0 };
	uint64_t                            currentFenceValue = 1;
	HANDLE                              fenceEvent = nullptr;

	~CommandQueueContext() {
		if (fenceEvent) {
			CloseHandle(fenceEvent);
			fenceEvent = nullptr;
		}
	}
};

class QueueManager {
public:
	QueueManager() = delete;
	QueueManager(ID3D12Device4* device) :m_device(device) {};
	~QueueManager() = default;

	void Init();

	void BeginCommandRecording(QueueType queue, uint32_t currentFrame);
	void EndAndExecuteCommandRecording(QueueType queue, uint32_t currentFrame);

	void WaitFrameReady(QueueType queue, uint32_t currentFrame);
	void SignalCurrentFrame(QueueType queue, uint32_t currentFrame);
	void FlushCommandQueue(QueueType queue);

	ID3D12CommandQueue*			GetQueue(QueueType queue);
	ID3D12GraphicsCommandList1*	GetList(QueueType queue);

private:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;
	ComPtr<ID3D12Device4> m_device;

	std::vector<CommandQueueContext> m_commandQueues;

	void InitializeCommandQueue();
	void InitializeCommandList();
};