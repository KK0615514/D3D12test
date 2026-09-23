#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放
#include <DirectXMath.h>
#include <array>

#include "CommonUtils.h"

//test
#include "MeshData.h"

using Microsoft::WRL::ComPtr;

enum class PsoType {
	TextureLoader,
	TextLoader,
	MeshDebug,
	Count
};

class PipelineManager {
public:
	PipelineManager() = delete;
	PipelineManager(ID3D12Device4* device , ID3D12RootSignature* rootSignature)
		: m_device(device) , m_rootSignature(rootSignature){};

	void Init();
	ID3D12PipelineState* GetPso(PsoType type);

private:
	ComPtr<ID3D12Device4>															m_device;
	ComPtr<ID3D12RootSignature>														m_rootSignature;

	std::array<ComPtr<ID3D12PipelineState> , static_cast<size_t>(PsoType::Count)>	m_psos;

	void InitializePSO();

	//test
	void InitMeshPSO();
};