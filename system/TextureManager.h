#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <vector>

using Microsoft::WRL::ComPtr;

struct TextureHandle {
	uint32_t id; // 或者叫 descriptorIndex [1]
};

struct TextureData{	
	ComPtr<ID3D12Resource>	 resource;			
	uint32_t				 descriptorIndex;						//srv編號
};

class TextureManager{
public:
	TextureManager() = delete;
	TextureManager(ID3D12Device4* device) :m_device(device) {}
	
	void Init();
	TextureHandle LoadTexture(const char* path, ID3D12GraphicsCommandList1* m_commandList);
	void ClearUploadBuffer();

	ComPtr<ID3D12DescriptorHeap> m_srvHeap;						//共用descriptorHeap

private:
	ComPtr<ID3D12Device4> m_device;

	
	uint32_t m_descriptorSize = 0;								//Init時取得srv Descriptor Heap大小
	uint32_t m_allocatedCount = 0;

	std::vector<TextureData> m_texturePool;						//物件池
	std::vector<ComPtr<ID3D12Resource>> m_tempUploadBuffers;	//Upload用的buffer scope內一定傳不完 所以保留 
};