#include "D3D12Engine.h"

void D3D12Engine::Init() {
    InitializeRootSignature();
    InitializeConstantBuffer();
    InitializeStructureBuffer();
}

void D3D12Engine::InitDevice() {
#if defined(_DEBUG)
    // Debug版本 Debug層啟用 針對shader部分除錯
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        //ComPtr<ID3D12Debug1> debugController1;                                     //會很卡 有破圖才開啟
        //if (SUCCEEDED(debugController.As(&debugController1))) {
        //    debugController1->SetEnableGPUBasedValidation(true);                   // 開啟GBV驗證
        //    debugController1->SetEnableSynchronizedCommandQueueValidation(true);   // (選填)同步偵錯模式 錯誤瞬間立刻攔截 方便除錯
        //}
    }
    // 傳入DXGI_CREATE_FACTORY_DEBUG 配合CreateFactory增加基礎結構除錯
    Common::ThrowIfFailed(
        CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_factory)),
        "Factory建立失敗"
    );

#else
    // Release版本 不啟用Debug層
    ThrowIfFailed(
        CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory)), 
        "Factory建立失敗"
    );

#endif

    // 建立Adapter 選顯卡
    Common::ThrowIfFailed(
        m_factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&m_adapter)),
        "Adapter建立失敗"
    );
    // 建立Device
    Common::ThrowIfFailed(
        D3D12CreateDevice(m_adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&m_device)),
        "Device建立失敗"
    );

}

void D3D12Engine::InitSwapChain(HWND hwnd, ID3D12CommandQueue* Queue){
    // 建立SwapChain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};       //無印已被淘汰
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    //swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    
    ComPtr<IDXGISwapChain1> swapChain;
    Common::ThrowIfFailed(
        m_factory->CreateSwapChainForHwnd(Queue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain),
        "轉換用SwapChain建立失敗"
    );
    swapChain.As(&m_swapChain);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 建立RTV Descriptor Heap (地址串)
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Common::ThrowIfFailed(
        m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
        "RTV Descriptor Heap建立失敗"
    );
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);   //取得顯卡的rtv大小

    // 建立RTV (綁定buffer與handle)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < FrameCount; i++)
    {
        m_swapChain->GetBuffer(i,IID_PPV_ARGS(& m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
}

void D3D12Engine::InitializeRootSignature() {
    //slot的規格 & 設定descriptor的範圍		可以用vector來一次上傳多個descriptor
    D3D12_DESCRIPTOR_RANGE1 texRange{};
    texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange.NumDescriptors = UINT_MAX;                          // 設定 1 我必須使用for迴圈重複計算offset跟綁定資源 設定最大上限 即可在HLSL裡面使用內建index抓圖 
    texRange.BaseShaderRegister = 0;                        // t0 space0
    texRange.RegisterSpace = 0;
    texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;		//自動補齊 不用手動 算offset
    //test
    texRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

    //建立 root slot 參數
    D3D12_ROOT_PARAMETER1 rootParameter[3]{};
    //slot0 : texture srv 
    rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;      //包含一個Range 也就是一個descriptor heap
    rootParameter[0].DescriptorTable.pDescriptorRanges = &texRange;
    rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 圖片通常只在 Pixel Shader 使用
    //slot1 : sb srv
    rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameter[1].Descriptor.ShaderRegister = 0;         // t0, space1
    rootParameter[1].Descriptor.RegisterSpace = 1;
    rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    //slot2 : cbv
    rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter[2].Descriptor.ShaderRegister = 0; // b0, space0
    rootParameter[2].Descriptor.RegisterSpace = 0;
    rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 定義static sampler desc for root signature
    D3D12_STATIC_SAMPLER_DESC imageSampler{};
    // 1. 過濾模式：使用線性過濾（放大、縮小、Mipmap 皆為線性，即雙線性過濾）
    imageSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    // 2. 尋址模式：設定為 CLAMP（截取），防止圖片邊緣因重複而產生雜點
    imageSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    imageSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    imageSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    // 3. 額外參數設定（顯示一般圖片的標準預設值）
    imageSampler.MipLODBias = 0.0f;
    imageSampler.MaxAnisotropy = 1;
    imageSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    imageSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    imageSampler.MinLOD = 0.0f;
    imageSampler.MaxLOD = D3D12_FLOAT32_MAX;
    // 4. 著色器對應：對應到 HLSL 中的 s0 (Register 0)
    imageSampler.ShaderRegister = 0;
    imageSampler.RegisterSpace = 0;
    imageSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 僅像素著色器可見

    // 綁定至root signature desc	只顯示圖片 所以只有一個srv+sampler
    D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc{};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParameter;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &imageSampler; // 帶入靜態採樣器 首位址
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc{};
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDesc.Desc_1_1 = rootSigDesc;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    Common::ThrowIfFailed(
        D3D12SerializeVersionedRootSignature(&versionedDesc,&signatureBlob,&errorBlob),
        "Root Signature序列化失敗"
    );

    m_device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );
}

//移動到 新增的buffermanager
void D3D12Engine::InitializeConstantBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    uint32_t bufferSize = 4 * 1024; //每個cbv 上限64kb 效能為主4kb
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize; // 必須對齊256
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (uint32_t f = 0;f < FrameCount;++f)
    {
        for (uint32_t i = 0;i < MaxCBVsPerFrame;++i)
        {
            m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer[f][i])
            );
            D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
            m_constantBuffer[f][i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvCpuAdress[f][i]));
        }
    }
}

//移動到 新增的buffermanager
void D3D12Engine::InitializeStructureBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;                            //未來改成default 搭配一個upload buffer來每幀上傳物件資料
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1; 
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;                                               // 0 表示預設對齊（64KB）
    bufferDesc.Width = sizeof(InstanceData) * MAX_ELEMENTS;
    bufferDesc.Height = 1;        
    bufferDesc.DepthOrArraySize = 1; 
    bufferDesc.MipLevels = 1;    
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured Buffer必為UNKNOWN
    bufferDesc.SampleDesc.Count = 1;   
    bufferDesc.SampleDesc.Quality = 0; 
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; 
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;        

    for (uint32_t i = 0; i < FrameCount; i++)
    {
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_structureBuffer[i])
        );
        D3D12_RANGE readRange = { 0, 0 }; // CPU 不需要讀取這段記憶體 設為空
        m_structureBuffer[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_structureBufferCpuAddress[i]));
    }

}

//會保留 但目前是臨時用的
void D3D12Engine::Update(const IScene& currentScene) {
    UpdatePerFrameCB(currentScene);
    UpdateInstanceSB(currentScene);
}

//移動到 新增的buffermanager
void D3D12Engine::UpdatePerFrameCB(const IScene& currentScene) {
    float windowWidth = 1280.0f;
    float windowHeight = 720.0f;

    //寫死的camera
    DirectX::XMFLOAT2 camPos = { 0.0f, 0.0f };   // 相看著世界坐標的 (0,0)
    float camRotation = 0.0f;                    // 角度
    float camZoom = 1.0f;                        // 縮放倍率

    DirectX::XMMATRIX projMat = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, windowWidth, windowHeight, 0.0f, -1.0f, 1.0);
    DirectX::XMMATRIX viewMat = DirectX::XMMatrixIdentity();                        //單位矩陣
    DirectX::XMMATRIX viewProjMat = DirectX::XMMatrixMultiply(viewMat, projMat);    //矩陣乘法

    PerFrameConstant cbData;
    DirectX::XMStoreFloat4x4(&cbData.View, DirectX::XMMatrixTranspose(viewMat));
    DirectX::XMStoreFloat4x4(&cbData.Proj, DirectX::XMMatrixTranspose(projMat));
    DirectX::XMStoreFloat4x4(&cbData.ViewProj, DirectX::XMMatrixTranspose(viewProjMat));
    cbData.CameraPos = DirectX::XMFLOAT3(camPos.x, camPos.y, 0.0f);
    cbData.padding = 0.0f;

    //cbv裡面只有一個物件 不用對齊
    memcpy(m_cbvCpuAdress[m_frameIndex][0], &cbData, sizeof(cbData));
}

//移動到 新增的buffermanager
void D3D12Engine::UpdateInstanceSB(const IScene& currentScene) {
    memcpy(m_structureBufferCpuAddress[m_frameIndex],
        currentScene.InstanceDatas.data(),
        currentScene.InstanceDatas.size() * sizeof(InstanceData)
    );
}
void D3D12Engine::UpdateSwapChainBackBufferIndex() {
    m_swapChain->Present(0, 0);//DXGI_PRESENT_ALLOW_TEARING
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}