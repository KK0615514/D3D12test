#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include "CommonUtils.h"

//test			可刪除
#include "../scene/GameScene.h"

//移動進buffer manager			可刪除
#include "ShaderStructureBuffer.h"
#include "ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

class D3D12Engine {
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;
	
	//核心硬體物件
	ComPtr<IDXGIFactory6>				m_factory;				// 4 必須撈出所有顯卡後依需求計算要哪張 6支援關鍵字搜尋
	ComPtr<IDXGIAdapter4>				m_adapter;				// 我也看不懂差在哪裡
	ComPtr<ID3D12Device4>				m_device;				// IDXGIDevice4很陷阱 是D3D12 不是DXGI
	ComPtr<IDXGISwapChain4>				m_swapChain;			// 2支援幀延遲 3支援不重置buffer的條件下Resize螢幕

	//Descriptor Heaps
	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
	uint32_t							m_rtvDescriptorSize = 0;						//UINT = uint32_t , UINT64 = uint64_t 前者WINDOW 後者跨平台
	ComPtr<ID3D12Resource>				m_renderTargets[FrameCount];					// Resource 應付95%場景 Resource2/3可配合DirectStorage

	//當前SwapChain的Buffer編號
	uint32_t							m_frameIndex = 0;

	uint32_t							m_height = 720;
	uint32_t							m_width = 1280;

	//Root Signature
	ComPtr<ID3D12RootSignature>			m_rootSignature;

	//cbv
	static const uint32_t				MaxCBVsPerFrame = 3;						//每個Frame最多幾個cbv
	ComPtr<ID3D12Resource>				m_constantBuffer[FrameCount][MaxCBVsPerFrame];
	void*								m_cbvCpuAdress[FrameCount][MaxCBVsPerFrame] = {nullptr};

	//sturcture buffer
	const size_t						MAX_ELEMENTS = 100000;      // 物件上限
	ComPtr<ID3D12Resource>				m_structureBuffer[FrameCount];
	InstanceData*						m_structureBufferCpuAddress[FrameCount] = { nullptr };

public:
	D3D12Engine() = default;
	~D3D12Engine() = default;

	void InitRootSig();
	void InitDevice();
	void InitSwapChain(HWND hwnd, ID3D12CommandQueue* Queue);

	void Update();
	void Shutdown();

	void UpdateSwapChainBackBufferIndex();

private:

};