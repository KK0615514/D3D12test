#pragma once
#include <d3d12.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "system/HeapManager.h"
#include "system/UploadManager.h"	

#include "system/DescriptorManager.h"
#include "system/RenderHandle.h"

using Microsoft::WRL::ComPtr;

// 物件資料(含CPU Handle)
struct TextureGpuResource {
	ComPtr<ID3D12Resource> resource;
	HeapAllocation allocation{};

	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

	DescriptorSlot srv{};
	DescriptorSlot uav{};
	bool hasUav = false;
};

// 檢查Texture是否是新載入的
struct TextureLoadResult {
	TextureHandle handle;
	bool newlyCreated;
};

struct TextureUploadCheckpoint {
	std::size_t textureCount = 0;
	std::size_t pendingCount = 0;
};



class TextureManager{
public:
	TextureManager() = delete;
	TextureManager(ID3D12Device4* device, HeapManager* heapManager, UploadManager* uploadManager, DescriptorManager* descriptors)
		: m_device(device),m_heapManager(heapManager),m_uploadManager(uploadManager),m_descriptors(descriptors) {}
	~TextureManager() = default;
	
	TextureLoadResult LoadTexture(const char* name, ID3D12GraphicsCommandList1* copyList);
	TextureHandle CreateTexture2D(
		const std::string& name, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav = false);
	void ClearUploadBuffers();

	TextureUploadCheckpoint GetUploadCheckpoint() const;
	void RollbackUpload(const TextureUploadCheckpoint& checkpoint);

	void TransitionTexture(TextureHandle handle, ID3D12GraphicsCommandList1* directList, D3D12_RESOURCE_STATES targetState);
	void UavBarrier(TextureHandle handle, ID3D12GraphicsCommandList1* directList);

	uint32_t GetSrvIndex(TextureHandle handle) const;
	const TextureGpuResource* TryGetTexture(TextureHandle handle) const;
	const TextureGpuResource* TryGetTexture(const std::string& name) const;

private:
	ComPtr<ID3D12Device4>		m_device;
	HeapManager* 				m_heapManager = nullptr;
	UploadManager*				m_uploadManager = nullptr;
	DescriptorManager*			m_descriptors = nullptr;

	std::vector<TextureGpuResource >				m_textures;					// 實際持有資源
	std::unordered_map<std::string, TextureHandle>	m_textureCache;				// 對照表 檢查重複載入用

	std::vector<UploadSlice>						m_pendingUploadBuffers;		// Upload用的buffer scope內一定傳不完 所以保留 

	// 建立資源用內部入口
	TextureGpuResource  BuildTexture2D(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav);
	TextureHandle RegisterTexture(const std::string& name, TextureGpuResource&& data);

	// Barrier用內部入口
	static void RecordTransition(
		TextureGpuResource& texture, ID3D12GraphicsCommandList1* directList, D3D12_RESOURCE_STATES targetState);

	// 實際建立資源內部入口
	ComPtr<ID3D12Resource> CreateDefaultTexture(
		const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, HeapAllocation& allocation);
	UploadSlice StageUploadData(
		const D3D12_RESOURCE_DESC& desc,const unsigned char* sourceData, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint);
};