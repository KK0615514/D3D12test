#include "PipelineManager.h"

//test
#include "MeshData.h"
#include <cstddef>

void PipelineManager::Init() {
	InitializePSO();

	//test
	InitMeshPSO();
}

void PipelineManager::InitializePSO() {
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		// 1. 位置數據: 映射到頂點結構體的 position
		{
			"POSITION",                     // HLSL 中的語義名稱
			0,                              // 語義索引
			DXGI_FORMAT_R32G32B32_FLOAT,    // 資料格式 (3個 float, 剛好是 XMFLOAT3)
			0,                              // 輸入槽 (Input Slot)
			0,                              // 記憶體偏移量 (從第 0 位元組開始)
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// 2. UV 數據: 映射到頂點結構體的 texCoord
		{
			"TEXCOORD",                     // HLSL 中的語義名稱
			0,                              // 語義索引
			DXGI_FORMAT_R32G32_FLOAT,       // 資料格式 (2個 float, 剛好是 XMFLOAT2)
			0,                              // 輸入槽
			12,                             // 記憶體偏移量 (跳過前面的 Position 12 位元組)
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};
	auto vsData = Common::LoadBinaryFile(L"shader/TextureLoader_vs.cso");
	auto psData = Common::LoadBinaryFile(L"shader/TextureLoader_ps.cso");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_rootSignature.Get();            // 綁定根簽名
	psoDesc.VS.pShaderBytecode = vsData.data();
	psoDesc.VS.BytecodeLength = vsData.size();
	psoDesc.PS.pShaderBytecode = psData.data();
	psoDesc.PS.BytecodeLength = psData.size();
	// D3D12 其實有提供內建的設定函式，可以不用手動打
	psoDesc.RasterizerState = {}; // 先清空
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.DepthClipEnable = TRUE; // 最核心需要的
	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.IndependentBlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE; // 開啟透明度混合
	// 色彩混合公式：最終顏色 = (圖片色彩 * 圖片Alpha) + (背景藍色 * (1 - 圖片Alpha))
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

	// 設定 Alpha 混合公式（通常維持原樣或與上方一致）
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.DepthStencilState.DepthEnable = FALSE;          // 2D 圖片通常關閉深度測試
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 畫三角形
	// 設定輸出緩衝區格式
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;     // 配合Swapchain格式
	psoDesc.SampleDesc.Count = 1;

	m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_psos[static_cast<int>(PsoType::TextureLoader)]));
}

//test
void PipelineManager::InitMeshPSO() {
	const D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		  static_cast<UINT>(offsetof(MeshVertex, position)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		  static_cast<UINT>(offsetof(MeshVertex, normal)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	auto vs = Common::LoadBinaryFile(L"shader/test_vs.cso");
	auto ps = Common::LoadBinaryFile(L"shader/test_ps.cso");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = m_rootSignature.Get();
	desc.InputLayout = { layout, _countof(layout) };
	desc.VS = { vs.data(), vs.size() };
	desc.PS = { ps.data(), ps.size() };

	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;

	Common::ThrowIfFailed(
		m_device->CreateGraphicsPipelineState(
			&desc, IID_PPV_ARGS(&m_psos[static_cast<size_t>(PsoType::MeshDebug)])),
		"MeshDebug PSO failed"
	);
}

ID3D12PipelineState* PipelineManager::GetPso(PsoType type){
	return m_psos[static_cast<size_t>(type)].Get();
}
