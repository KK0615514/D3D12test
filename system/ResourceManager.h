#pragma once
#include <d3d12.h>
#include <wrl/client.h>			// 使用 ComPtr 來自動管理 D3D12 指針釋放
	
#include <cstdint>				// uint16_t等
#include <memory>				

#include "system/TextureManager.h"
#include "system/MeshManager.h"
#include "system/DescriptorManager.h"

#include "system/CommonUtils.h"

#include "ecs_component/ShaderStructureBuffer.h"
#include "ecs_component/ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

// 向前宣告 渲染單子 
struct SceneRenderData;

//目前全部CommitResource
class ResourceManager {

//改成placedbuffer後清理下面的public
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	//cb sb之後改place再說 現在先這樣
	//constant buffer 
	static const uint32_t				MaxCBVsPerFrame = 3;							//每個Frame最多幾個cbv
	ComPtr<ID3D12Resource>				m_constantBuffer[FrameCount][MaxCBVsPerFrame];
	void* m_cbvCpuAdress[FrameCount][MaxCBVsPerFrame] = { nullptr };

	//sturcture buffer
	const size_t						MAX_ELEMENTS = 50000;      // 物件上限
	ComPtr<ID3D12Resource>				m_structureBuffer[FrameCount];
	InstanceData* m_structureBufferCpuAddress[FrameCount] = { nullptr };

public:
	ResourceManager(ID3D12Device4* device) :m_device(device) {};
	~ResourceManager() = default;

	void Init();
	void Update(const SceneRenderData& data, uint32_t currentFrame);

	DescriptorManager*	GetDescriptorManager();
	MeshManager*		GetMeshManager();
	TextureManager*		GetTextureManager();
	

public:			// AI copy paste
	static constexpr uint32_t OceanHeightResolution = 256;
	void InitializeOceanHeight();
	void PrepareOceanHeight(
		ID3D12GraphicsCommandList1* directList,
		ID3D12RootSignature* computeRootSignature,
		ID3D12PipelineState* computePso,
		const OceanWaveParameters& parameters);

	D3D12_GPU_DESCRIPTOR_HANDLE GetOceanHeightSrv() const {
		return m_oceanHeightSrv;
	}
private:		// AI copy paste
	ComPtr<ID3D12Resource> m_oceanHeight;

	D3D12_GPU_DESCRIPTOR_HANDLE m_oceanHeightSrv{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_oceanHeightUav{};

private:
	ComPtr<ID3D12Device4>			m_device;

	std::unique_ptr<DescriptorManager>	m_descriptorManager;
	std::unique_ptr<MeshManager>		m_meshManager;
	std::unique_ptr<TextureManager>		m_textureManager;

	void InitializeConstantBuffer();
	void InitializeStructureBuffer();

	void UpdatePerFrameCB(const SceneRenderData& data, uint32_t currentFrame);
	void UpdateInstanceSB(const SceneRenderData& data, uint32_t currentFrame);
};