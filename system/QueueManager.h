#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include "CommonUtils.h"

using Microsoft::WRL::ComPtr;

class QueueManager {
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	ComPtr<ID3D12CommandQueue>				m_commandQueue;
	ComPtr<ID3D12CommandAllocator>			m_commandAllocator[FrameCount];		//配合FrameCount數量設置Allocator			未來擴增為多執行緒版本	目前為單組allocator配合frameCount多緩衝
	ComPtr<ID3D12GraphicsCommandList1>		m_commandList;						//配合CreateCommandList1解藕allocator		未來擴增	cpp部分ppcommandlist修改為size

	ComPtr<ID3D12Fence>						m_commandQueueFence;				// 基本無印 多顯卡用1
	uint64_t								m_commandQueueFenceValue[2]{0};
	uint64_t								m_currentFenceValue = 1;
	HANDLE									m_commandQueueFenceEvent = nullptr;
public:
	QueueManager() = delete;
	QueueManager(ID3D12Device4* device) :m_device(device) {};
	~QueueManager() { CloseHandle(m_commandQueueFenceEvent); }

	void Init();

	void BeginCommandRecording(uint32_t currentFrame,ID3D12Resource* pBackBuffer);
	void EndAndExecuteCommandRecording(uint32_t currentFrame,ID3D12Resource* pBackBuffer);

	void WaitFrameReady(uint32_t currentFrame);
	void SignalCurrentFrame(uint32_t currentFrame);
	void FlushCommandQueue();

private:
	ComPtr<ID3D12Device4> m_device;

	void InitializeCommandQueue();
	void InitializeCommandList();


};