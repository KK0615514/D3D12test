#pragma once
#include <d3d12.h>
#include <wrl/client.h> // 使用 ComPtr 來自動管理 D3D12 指針釋放

#include <array>
#include <cstddef>

#include "system/PsoType.h"

using Microsoft::WRL::ComPtr;

class PipelineManager {
public:
	PipelineManager() = delete;
	PipelineManager(ID3D12Device4* device , ID3D12RootSignature* rootSignature)
		: m_device(device) , m_rootSignature(rootSignature){};
	~PipelineManager() = default;

	void Init();
	ID3D12PipelineState* GetPso(PsoType type) const;



public:			// AI copy paste
	ID3D12PipelineState* GetOceanComputePso() const {
		return m_oceanComputePso.Get();
	}
private:		// AI copy paste
	ComPtr<ID3D12PipelineState> m_oceanComputePso;
	void InitOceanComputePSO();



private:
	ComPtr<ID3D12Device4>															m_device;
	ComPtr<ID3D12RootSignature>														m_rootSignature;

	static constexpr size_t PsoCount = static_cast<size_t>(PsoType::Count);
	std::array<ComPtr<ID3D12PipelineState> , PsoCount>	m_psos;

	void InitializePSO();
	void InitMeshPSO();		
	void InitOceanPSO();
};