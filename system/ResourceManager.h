#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include "CommonUtils.h"

//test			注意調整後面func傳入參數 
#include "../scene/GameScene.h"

#include "ShaderStructureBuffer.h"
#include "ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

//目前全部CommitResource
class ResourceManager {
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	ComPtr<ID3D12Device4>				m_device;

	//constant buffer 
	static const uint32_t				MaxCBVsPerFrame = 3;						//每個Frame最多幾個cbv
	ComPtr<ID3D12Resource>				m_constantBuffer[FrameCount][MaxCBVsPerFrame];
	void*								m_cbvCpuAdress[FrameCount][MaxCBVsPerFrame] = { nullptr };

	//sturcture buffer
	const size_t						MAX_ELEMENTS = 100000;      // 物件上限
	ComPtr<ID3D12Resource>				m_structureBuffer[FrameCount];
	InstanceData*						m_structureBufferCpuAddress[FrameCount] = { nullptr };

public:
	ResourceManager(ID3D12Device4* device) :m_device(device) {};
	~ResourceManager() = default;

	void Init();
	void Update(const IScene& currentScene, uint32_t currentFrame);

private:
	void InitializeConstantBuffer();
	void InitializeStructureBuffer();

	void UpdatePerFrameCB(const IScene& currentScene,uint32_t currentFrame);
	void UpdateInstanceSB(const IScene& currentScene,uint32_t currentFrame);
};