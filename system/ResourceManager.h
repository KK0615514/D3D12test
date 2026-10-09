#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>			// 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <DirectXMath.h>
	
#include <string>				// 實作asset path cache
#include <cstring>				// 使用std::memcpy
#include <unordered_map>		// 實作asset path cache

#include "system/CommonUtils.h"
#include "system/MeshData.h"

#include "system/RenderHandle.h"

#include "ecs_component/ShaderStructureBuffer.h"
#include "ecs_component/ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

// 物件資料
struct MeshGpuResource {
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;

	ComPtr<ID3D12Resource> vertexUploadBuffer;
	ComPtr<ID3D12Resource> indexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
};

// 檢查mesh是否是新載入的
struct MeshLoadResult {
	MeshHandle handle;
	bool newlyCreated;
};

// 向前宣告 渲染單子 
struct SceneRenderData;

//目前全部CommitResource
class ResourceManager {

//改成placedbuffer後清理下面的public
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;

	//cb sb之後改place再說 現在先這樣
	//constant buffer 
	static const uint32_t				MaxCBVsPerFrame = 3;						//每個Frame最多幾個cbv
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

	MeshLoadResult LoadMesh(const char* path, ID3D12GraphicsCommandList1* m_commandList);
	void TransitionMeshBuffer(MeshHandle handle, ID3D12GraphicsCommandList1* m_commandList);
	void ClearUploadBuffer();

	const MeshGpuResource* TryGetMesh(MeshHandle handle) const;	//採用指標 方便檢測 因為可以nullptr回傳

	// test
	MeshHandle LoadOceanMesh(const OceanMeshData& meshData, ID3D12GraphicsCommandList1* copyList);

private:
	ComPtr<ID3D12Device4>			m_device;
	std::vector<MeshGpuResource>	m_meshes;
	std::unordered_map<std::string, MeshHandle> m_meshCache;

	void InitializeConstantBuffer();
	void InitializeStructureBuffer();

	void UpdatePerFrameCB(const SceneRenderData& data, uint32_t currentFrame);
	void UpdateInstanceSB(const SceneRenderData& data, uint32_t currentFrame);
};