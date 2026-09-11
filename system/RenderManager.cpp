#include "RenderManager.h"

void RenderManager::Init() {
	InitializeVertexBuffer();
	InitializeIndexBuffer();
	InitializePSO();

	Draw();
}

void RenderManager::InitializeVertexBuffer() {
	//建立Vertex Buffer
	const uint32_t vertexBufferSize = MAX_VERTEX_COUNT * sizeof(Vertex);

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;		// CPU 可寫入的中轉堆疊

	D3D12_RESOURCE_DESC bufferDesc{};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = vertexBufferSize; // 緩衝區總大小
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	m_device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // Upload Heap 必須是這個狀態
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)
	);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();		// GPU 記憶體首地址
	m_vertexBufferView.SizeInBytes = vertexBufferSize;		// 緩衝區總大小
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
}
	
void RenderManager::InitializeIndexBuffer() {
	//建立Index Buffer
	const uint32_t indexBufferSize = MAX_INDEX_COUNT * sizeof(uint32_t);

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;		// CPU 可寫入的中轉堆疊

	D3D12_RESOURCE_DESC indexbufferDesc{};
	indexbufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	indexbufferDesc.Width = indexBufferSize; // 緩衝區總大小
	indexbufferDesc.Height = 1;
	indexbufferDesc.DepthOrArraySize = 1;
	indexbufferDesc.MipLevels = 1;
	indexbufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	indexbufferDesc.SampleDesc.Count = 1;
	indexbufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	m_device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&indexbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // Upload Heap 的預設安全狀態
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)
	);
	//事先寫好等同於MAX_SPRITES份量的索引
	uint32_t* pIndexDataBegin = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
	m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
	for (size_t i = 0;i < MAX_SPRITES; i++)
	{
		size_t vOffset = i * 4; // 每個物件佔用 4 個頂點
		size_t iOffset = i * 6; // 每個物件佔用 6 個索引

		// 三角形 1
		pIndexDataBegin[iOffset + 0] = static_cast<uint32_t>(vOffset + 0); //左上
		pIndexDataBegin[iOffset + 1] = static_cast<uint32_t>(vOffset + 1); //右上
		pIndexDataBegin[iOffset + 2] = static_cast<uint32_t>(vOffset + 2); //右下

		// 三角形 2
		pIndexDataBegin[iOffset + 3] = static_cast<uint32_t>(vOffset + 2); //右下
		pIndexDataBegin[iOffset + 4] = static_cast<uint32_t>(vOffset + 3); //左下
		pIndexDataBegin[iOffset + 5] = static_cast<uint32_t>(vOffset + 0); //左上
	}
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();	// GPU 記憶體首地址
	m_indexBufferView.SizeInBytes = indexBufferSize;							// 緩衝區總大小
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

//併入 line11 目前在8	
//目前的 點順序是反的 會觸發背面剔除 現在順時針 逆時針為主流 右手定則
void RenderManager::Draw() {
	Vertex* pVertexDataBegin = nullptr;
	D3D12_RANGE readRange = { 0, 0 };				// CPU 不需要讀取這段記憶體 設為空
	m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
	size_t activeSpriteCount = 1;					// 只放一個四方形來貼圖
	for (size_t i = 0; i < activeSpriteCount; i++)
	{
		size_t vOffset = i * 4; 

		pVertexDataBegin[vOffset + 0] = Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f } }; // 左上 
		pVertexDataBegin[vOffset + 1] = Vertex{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } }; // 右上 
		pVertexDataBegin[vOffset + 2] = Vertex{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f } }; // 右下 
		pVertexDataBegin[vOffset + 3] = Vertex{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 1.0f } }; // 左下
	}
	m_vertexBuffer->Unmap(0, nullptr);
}

void RenderManager::InitializePSO() {
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

ID3D12PipelineState* RenderManager::GetPso(PsoType type){
	return m_psos[static_cast<size_t>(type)].Get();
}
