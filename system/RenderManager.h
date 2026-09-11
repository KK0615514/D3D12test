#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <DirectXMath.h>
#include <array>

#include "CommonUtils.h"

using Microsoft::WRL::ComPtr;

struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 uv;
};

enum class PsoType {
	TextureLoader,
	TextLoader,
	Count
};

class RenderManager {
public:
	RenderManager() = delete;
	RenderManager(ID3D12Device4* device , ID3D12RootSignature* rootSignature) 
		: m_device(device) , m_rootSignature(rootSignature){};

	void Init();
	void Draw();
	ID3D12PipelineState* GetPso(PsoType type);

	D3D12_VERTEX_BUFFER_VIEW														m_vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW															m_indexBufferView{};
private:
	ComPtr<ID3D12Device4>															m_device;
	ComPtr<ID3D12RootSignature>														m_rootSignature;

	//移動到buffer manager
	ComPtr<ID3D12Resource>															m_vertexBuffer;
	ComPtr<ID3D12Resource>															m_indexBuffer;

	std::array<ComPtr<ID3D12PipelineState> , static_cast<size_t>(PsoType::Count)>	m_psos;

	const size_t MAX_SPRITES = 1;											//上限1個物件 兩個三角形組合的mesh
	const size_t MAX_VERTEX_COUNT = MAX_SPRITES * 4;						//兩個三角形 四個頂點
	const size_t MAX_INDEX_COUNT = MAX_SPRITES * 6;							//六個Index

	void InitializeVertexBuffer();
	void InitializeIndexBuffer();
	void InitializePSO();
};