#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <DirectXMath.h>

#include "CommonUtils.h"
#include "MeshData.h"

#include "scene/IScene.h"

#include "ecs_component/ShaderStructureBuffer.h"
#include "ecs_component/ShaderConstantBuffer.h"

using Microsoft::WRL::ComPtr;

// 物件handle
struct MeshHandle {
	uint32_t id; // 或者叫 descriptorIndex [1]
};

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

//目前全部CommitResource
class ResourceManager {

//改成placedbuffer後清理下面的public
public:
	static constexpr uint32_t FrameCount = Common::BackBufferCount;
	//constant buffer 
	static const uint32_t				MaxCBVsPerFrame = 3;						//每個Frame最多幾個cbv
	ComPtr<ID3D12Resource>				m_constantBuffer[FrameCount][MaxCBVsPerFrame];
	void* m_cbvCpuAdress[FrameCount][MaxCBVsPerFrame] = { nullptr };

	//sturcture buffer
	const size_t						MAX_ELEMENTS = 10000;      // 物件上限
	ComPtr<ID3D12Resource>				m_structureBuffer[FrameCount];
	InstanceData* m_structureBufferCpuAddress[FrameCount] = { nullptr };
public:
	ResourceManager(ID3D12Device4* device) :m_device(device) {};
	~ResourceManager() = default;

	void Init();
	void Update(const IScene& currentScene, uint32_t currentFrame);

	MeshHandle Load2DMesh(ID3D12GraphicsCommandList1* m_commandList);		//載入2D用的 VB*4 IB*6 
	MeshHandle LoadMesh(const char* path, ID3D12GraphicsCommandList1* m_commandList);
	void TransitionMeshBuffer(std::vector<MeshGpuResource>meshes, ID3D12GraphicsCommandList1* m_commandList);
	void ClearUploadBuffer();

	const MeshGpuResource& GetMesh(MeshHandle handle) const;

private:
	ComPtr<ID3D12Device4>			m_device;
	std::vector<MeshGpuResource>	m_meshes;

	void InitializeConstantBuffer();
	void InitializeStructureBuffer();

	void UpdatePerFrameCB(const IScene& currentScene,uint32_t currentFrame);
	void UpdateInstanceSB(const IScene& currentScene,uint32_t currentFrame);
};