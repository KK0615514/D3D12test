#pragma once
#include <d3d12.h>
#include <wrl/client.h>			// 使用 ComPtr 來自動管理 D3D12 指針釋放
	
#include <cstdint>				// uint16_t等
#include <memory>				
#include <cstddef>

#include "system/HeapManager.h"
#include "system/UploadManager.h"
#include "system/DescriptorManager.h"
#include "system/MeshManager.h"
#include "system/TextureManager.h"

#include "system/CommonUtils.h"

#include "ecs_component/ShaderStructureBuffer.h"
#include "ecs_component/ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

struct MappedBufferGpuResource {
	ComPtr<ID3D12Resource> resource;
	HeapAllocation allocation{};
	unsigned char* cpuAddress = nullptr;
};

// 向前宣告 渲染單子 
struct SceneRenderData;



class ResourceManager {
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	ResourceManager(ID3D12Device4* device, IDXGIAdapter4* adapter)
		:m_device(device),m_adapter(adapter){};
	~ResourceManager() = default;

	void Init();
	void Update(const SceneRenderData& data, uint32_t currentFrame);

	void BeginUploadBatch();
	void MarkUploadSubmissionStarted();					// 從這裡開始，這批資源不再允許 rollback。
	void CommitUploadBatch(uint64_t copyFenceValue);
	void RollbackUploadBatch() noexcept;

	D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress(uint32_t currentFrame, uint32_t slot = 0) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetStructuredBufferAddress(uint32_t currentFrame) const;

	HeapManager*		GetHeapManager();
	UploadManager*		GetUploadManager();
	DescriptorManager*	GetDescriptorManager();
	MeshManager*		GetMeshManager();
	TextureManager*		GetTextureManager();

private:
	ComPtr<ID3D12Device4>			m_device;
	ComPtr<IDXGIAdapter4>			m_adapter;

	std::unique_ptr<HeapManager>		m_heapManager;
	std::unique_ptr<UploadManager>		m_uploadManager;
	std::unique_ptr<DescriptorManager>	m_descriptorManager;
	std::unique_ptr<MeshManager>		m_meshManager;
	std::unique_ptr<TextureManager>		m_textureManager;



	bool m_uploadBatchActive = false;
	bool m_uploadSubmissionStarted = false;

	uint32_t m_descriptorUploadCheckpoint = 0;
	MeshUploadCheckpoint m_meshUploadCheckpoint{};
	TextureUploadCheckpoint m_textureUploadCheckpoint{};



	static constexpr uint32_t MaxCBVsPerFrame = 3;
	static constexpr uint64_t ConstantBufferSize = 4 * 1024;
	static constexpr std::size_t MaxInstanceCount = 50000;

	MappedBufferGpuResource m_constantBuffers[FrameCount][MaxCBVsPerFrame]{};
	MappedBufferGpuResource m_structuredBuffers[FrameCount]{};

	MappedBufferGpuResource CreateUploadBuffer(uint64_t size);

	void InitConstantBuffers();
	void InitStructuredBuffers();

	void UpdatePerFrameCB(const SceneRenderData& data, uint32_t currentFrame);
	void UpdateInstanceSB(const SceneRenderData& data, uint32_t currentFrame);
};