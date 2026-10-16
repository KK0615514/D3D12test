#pragma once
#include <d3d12.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

#include "system/CommonUtils.h"

using Microsoft::WRL::ComPtr;

enum class QueueType: size_t{
	Direct	= 0,
	Copy	= 1,
	Compute = 2,
	Count
};

struct UploadCommandContext {
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList1> commandList;

	// GPU 完成到此值，才能重用 allocator。
	uint64_t retireFenceValue = 0;	// 0：尚未提交；UINT64_MAX：禁止重用。

	bool isClosed = false;
};

struct CommandQueueContext
{
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	// Queue 與命令列表核心
	ComPtr<ID3D12CommandQueue>          commandQueue{};
	ComPtr<ID3D12CommandAllocator>      commandAllocators[FrameCount]{};	// 配合FrameCount數量設置Allocator			未來擴增為多執行緒版本	目前為單組allocator配合frameCount多緩衝
	ComPtr<ID3D12GraphicsCommandList1>  commandList{};						// 配合CreateCommandList1解藕allocator		未來擴增	cpp部分ppcommandlist修改為size

	// Fence
	ComPtr<ID3D12Fence>                 fence{};							// 基本無印 多顯卡用1
	uint64_t                            fenceValues[FrameCount]{ 0 };
	uint64_t                            currentFenceValue = 1;				// for CPU
	HANDLE                              fenceEvent = nullptr;

	std::vector<UploadCommandContext> uploadContexts;
	uint32_t activeUploadContext = UINT32_MAX;		// 同一條 queue 同時只準備一份上傳 command list。

	~CommandQueueContext();
};



class QueueManager {
public:
	QueueManager() = delete;
	QueueManager(ID3D12Device4* device) :m_device(device) {};
	~QueueManager() = default;

	void Init();

	void BeginCommandRecording(QueueType queue, uint32_t currentFrame);
	void EndAndExecuteCommandRecording(QueueType queue, uint32_t currentFrame);

	// 幀末fence
	void WaitFrameReady(QueueType queue, uint32_t currentFrame);
	void SignalCurrentFrame(QueueType queue, uint32_t currentFrame);
	void FlushCommandQueue(QueueType queue);

	// 跨queue fence
	uint64_t Signal(QueueType queue);
	void QueueWait(QueueType waitingQueue,QueueType sourceQueue,uint64_t fenceValue);

	// 不等待 GPU。
	ID3D12GraphicsCommandList1* BeginUploadRecording(QueueType queue);	// 取得可重用的 context；沒有可用者時建立新的。
	void CloseUploadRecording(QueueType queue);
	uint64_t ExecuteUploadRecording(QueueType queue);					// 提交已 Close 的 list，並回傳此 queue 的完成 fence。
	void CancelUploadRecording(QueueType queue);				// 只丟棄尚未 Execute 的 list。

	// getter
	ID3D12CommandQueue*			GetQueue(QueueType queue) const;
	ID3D12GraphicsCommandList1*	GetList(QueueType queue) const;
	uint64_t					GetCompletedFenceValue(QueueType queue) const;

private:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	ComPtr<ID3D12Device4> m_device;

	static constexpr size_t QueueCount = static_cast<size_t>(QueueType::Count);
	std::array<CommandQueueContext, QueueCount> m_commandQueues;

	void InitCommandQueue();
	void InitCommandList();
};