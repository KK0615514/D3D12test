#pragma once
#include <d3d12.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "system/DescriptorManager.h"
#include "system/RenderHandle.h"

using Microsoft::WRL::ComPtr;

// 物件資料(含CPU)
struct TextureGpuResource {
	ComPtr<ID3D12Resource> resource;
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

class TextureManager{
public:
	TextureManager() = delete;
	TextureManager(ID3D12Device4* device, DescriptorManager* descriptors)
		: m_device(device),m_descriptors(descriptors) {}
	~TextureManager() = default;
	
	TextureLoadResult LoadTexture(const char* name, ID3D12GraphicsCommandList1* m_commandList);
	TextureHandle CreateTexture2D(
		const std::string& name, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav = false);
	void ClearUploadBuffer();

	const TextureGpuResource* TryGetTexture(TextureHandle handle) const;
	const TextureGpuResource* TryGetTexture(const std::string& name) const;
	uint32_t GetSrvIndex(TextureHandle handle) const;

private:
	ComPtr<ID3D12Device4>		m_device;
	DescriptorManager*			m_descriptors = nullptr;

	std::vector<TextureGpuResource >				m_textures;				//物件池
	std::unordered_map<std::string, TextureHandle>	m_textureCache;

	std::vector<ComPtr<ID3D12Resource>>				m_pendingUploadBuffers;		//Upload用的buffer scope內一定傳不完 所以保留 

	TextureGpuResource  BuildTexture2D(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav);
	TextureHandle RegisterTexture(const std::string& name, TextureGpuResource&& data);
};