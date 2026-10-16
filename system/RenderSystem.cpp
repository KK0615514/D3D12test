#include "RenderSystem.h"

#include <cstddef>
#include <stdexcept>

#include "system/RenderData.h"

void RenderSystem::Init(HWND hwnd) {
    m_d3d12Engine = std::make_unique<D3D12Engine>();
    m_d3d12Engine->InitDevice();      //queue需要device swapchain需要queue 逼不得已
    ID3D12Device4* m_device = m_d3d12Engine->GetDevice();

    m_queueManager = std::make_unique<QueueManager>(m_device);
    m_queueManager->Init();

    m_d3d12Engine->InitSwapChain(hwnd, m_queueManager->GetQueue(QueueType::Direct));
    m_d3d12Engine->InitDepthBuffer();
    m_d3d12Engine->InitRootSig();
    ID3D12RootSignature* m_rootsig = m_d3d12Engine->GetRootsig();

    m_resourceManager = std::make_unique<ResourceManager>(m_device, m_d3d12Engine->GetAdapter());
    m_resourceManager->Init();

    //預設只有一種rootsig
    m_pipelineManager = std::make_unique<PipelineManager>(m_device, m_rootsig);
    m_pipelineManager->Init();

    InitViewPortAndScissorRect();

    InitOceanHeight();
}

// 之後隨著視窗變形 或是想要啥鏡頭效果會需要提供 set這兩項的功能
void RenderSystem::InitViewPortAndScissorRect() {
    screenViewport.TopLeftX = 0.0f;
    screenViewport.TopLeftY = 0.0f;
    screenViewport.Width    = Common::InitialWindowWidth;
    screenViewport.Height   = Common::InitialWindowHeight;
    screenViewport.MinDepth = 0.0f;
    screenViewport.MaxDepth = 1.0f;

    scissorRect.left        = 0;
    scissorRect.top         = 0;
    scissorRect.right       = static_cast<uint32_t>(Common::InitialWindowWidth);
    scissorRect.bottom      = static_cast<uint32_t>(Common::InitialWindowHeight);
}



// 資源載入
MeshHandle RenderSystem::LoadOceanMesh(const OceanMeshData& meshData) {
    if (!m_d3d12Engine || !m_queueManager || !m_resourceManager || m_uploadSubmissionFailed) {
        throw std::logic_error("RenderSystem::LoadOceanMesh : RenderSystem 尚未初始化");
    }

    auto* meshes = m_resourceManager->GetMeshManager();
    m_resourceManager->BeginUploadBatch();

    MeshHandle handle{ UINT32_MAX };
    try {
        auto* copyList = m_queueManager->BeginUploadRecording(QueueType::Copy);
        handle = meshes->LoadOceanMesh(meshData,copyList);

        auto* directList = m_queueManager->BeginUploadRecording(QueueType::Direct);
        meshes->TransitionMeshBuffers(handle, directList);
    }
    catch (...) {
        CancelUploadBatch();
        throw;
    }
    SubmitUploadBatch();

    return handle;
}

std::vector<MeshHandle> RenderSystem::LoadMeshes(const std::vector<std::string>& names) {
    if (!m_d3d12Engine || !m_queueManager || !m_resourceManager || m_uploadSubmissionFailed) {
        throw std::logic_error("RenderSystem::LoadMeshes : RenderSystem 尚未初始化");
    }

    std::vector<MeshHandle> handles;
    if (names.empty()) {
        return handles;
    }
    handles.reserve(names.size());

    std::vector<MeshLoadResult> results;
    results.reserve(names.size());

    auto* meshes = m_resourceManager->GetMeshManager();
    m_resourceManager->BeginUploadBatch();

    try {
        auto* copyList = m_queueManager->BeginUploadRecording(QueueType::Copy);
        for (const auto& name : names) {
            const MeshLoadResult result = meshes->LoadMesh(name.c_str(), copyList);

            results.emplace_back(result);
            handles.push_back(result.handle);
        }

        auto* directList = m_queueManager->BeginUploadRecording(QueueType::Direct);
        for (const auto& result : results) {
            if (!result.newlyCreated) {
                continue;
            }

            meshes->TransitionMeshBuffers(result.handle, directList);
        }
    }
    catch (...) {
        CancelUploadBatch();
        throw;
    }

    SubmitUploadBatch();

    return handles;
}

std::vector<TextureHandle> RenderSystem::LoadTextures(const std::vector<std::string>& names) {
    if (!m_d3d12Engine || !m_queueManager || !m_resourceManager || m_uploadSubmissionFailed) {
        throw std::logic_error("RenderSystem::LoadTextures : RenderSystem 尚未初始化");
    }

    std::vector<TextureHandle> handles;
    if (names.empty()) {
        return handles;
    }
    handles.reserve(names.size());

    std::vector<TextureLoadResult> results;
    results.reserve(names.size());

    auto* textures = m_resourceManager->GetTextureManager();
    m_resourceManager->BeginUploadBatch();

    try {
        auto* copyList = m_queueManager->BeginUploadRecording(QueueType::Copy);
        for (const auto& name : names) {
            const TextureLoadResult result = textures->LoadTexture(name.c_str(), copyList);

            results.emplace_back(result);
            handles.push_back(result.handle);
        }

        auto* directList = m_queueManager->BeginUploadRecording(QueueType::Direct);
        for (const auto& result : results) {
            if (!result.newlyCreated) {
                continue;
            }

            textures->TransitionTexture(result.handle, directList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
    }
    catch (...) {
        CancelUploadBatch();
        throw;
    }

    SubmitUploadBatch();

    return handles;
}



void RenderSystem::CancelUploadBatch() noexcept {
    // 先丟棄錄製指令，再撤回它們引用的新資源。
    m_queueManager->CancelUploadRecording(QueueType::Direct);
    m_queueManager->CancelUploadRecording(QueueType::Copy);

    m_resourceManager->RollbackUploadBatch();
}

void RenderSystem::SubmitUploadBatch() {
    // 先讓兩份 list 都 Close 成功。
    // 此階段尚未送給 GPU，因此失敗可以取消。
    try {
        m_queueManager->CloseUploadRecording(QueueType::Copy);
        m_queueManager->CloseUploadRecording(QueueType::Direct);
    }
    catch (...) {
        CancelUploadBatch();
        throw;
    }

    try {
        m_resourceManager->MarkUploadSubmissionStarted();
        const uint64_t waitForCopy = m_queueManager->ExecuteUploadRecording(QueueType::Copy);
        m_resourceManager->CommitUploadBatch(waitForCopy);

        m_queueManager->QueueWait(QueueType::Direct, QueueType::Copy, waitForCopy);
        m_queueManager->ExecuteUploadRecording(QueueType::Direct);
    }
    catch (...) {
        // 可能已有工作送給 GPU。
        // 保留資源，不做 rollback；阻止繼續錄製畫面。
        m_uploadSubmissionFailed = true;
        throw;
    }
}



// 現在每幀fence等待是在這裡 之後要修改 不然太醜
void RenderSystem::Update(const SceneRenderData& data) {
    if (m_uploadSubmissionFailed) {
        throw std::logic_error("RenderSystem::Update : 上傳提交已失敗，不能繼續更新");
    }
#if defined(_DEBUG)
    if (m_memoryBudgetQueryCounter == 0) {
        m_localMemoryInfo = m_resourceManager->GetHeapManager()->GetVideoMemoryInfo();
        m_memoryBudgetQueryCounter = 60;
    }
    --m_memoryBudgetQueryCounter;
#endif

    const uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();
    m_resourceManager->GetUploadManager()->Collect(
        m_queueManager->GetCompletedFenceValue(QueueType::Copy)
    );

    // 一幀的開始 啟用direct queue
    m_queueManager->WaitFrameReady(QueueType::Direct, currentFrame);

    // 等待前一幀Fence完成後才更新CB SB    避免極大量物件時cmdlist還在讀取資料 然後Buffer就更新了
    // 我猜測 之前80000大便會閃爍就是Buffer還沒讀取畫完就更新下一幀的資料 因此造成閃爍
    m_resourceManager->Update(data, currentFrame);       
}

void RenderSystem::Render(const SceneRenderData& data) {
    if (m_uploadSubmissionFailed) {
        throw std::logic_error("RenderSystem::Render : 上傳提交已失敗，不能繼續錄製");
    }

    const uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();

    m_queueManager->BeginCommandRecording(QueueType::Direct, currentFrame);

    BindGlobalRenderResource(currentFrame);
    // test 畫海
    PrepareOceanHeight(data.oceanWaveParameters);

    SetViewAndScissor();
    ClearBackBuffer(currentFrame);
    DrawMeshPass(data);
    TransitionBackBuffer(currentFrame);

    m_queueManager->EndAndExecuteCommandRecording(QueueType::Direct, currentFrame);
    m_queueManager->SignalCurrentFrame(QueueType::Direct, currentFrame);
    m_d3d12Engine->UpdateSwapChainBackBufferIndex();    // 呈現畫面&直接切換frame 不在這裡等signal 等下次輪換到這個frame時在另一個函式確認完成與否
}

// 主迴圈最後釋放資源
void RenderSystem::Shutdown() {
    m_queueManager->FlushCommandQueue(QueueType::Copy);
    m_queueManager->FlushCommandQueue(QueueType::Compute);
    m_queueManager->FlushCommandQueue(QueueType::Direct);
}



// Render流程 - 1 綁定各種資源
void RenderSystem::BindGlobalRenderResource(uint32_t currentFrame) {
    auto* cmdList = m_queueManager->GetList(QueueType::Direct);
    auto* descriptors = m_resourceManager->GetDescriptorManager();
    ID3D12DescriptorHeap* shaderHeap = descriptors->GetShaderHeap();

    //指定texture 的 srv 
    cmdList->SetDescriptorHeaps(1, &shaderHeap);

    //每幀綁定root signature
    cmdList->SetGraphicsRootSignature(m_d3d12Engine->GetRootsig());

    //設定tex srv as descriptor table
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = descriptors->GetTableStart();
    cmdList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(RootSlot::TextureTable),
        textureSrvHandle
    );
    //設定sb srv as root descriptor
    D3D12_GPU_VIRTUAL_ADDRESS sbAddress = m_resourceManager->GetStructuredBufferAddress(currentFrame);
    cmdList->SetGraphicsRootShaderResourceView(
        static_cast<UINT>(RootSlot::InstanceBuffer),
        sbAddress
    );
    //設定cbv as root descriptor
    D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = m_resourceManager->GetConstantBufferAddress(currentFrame);
    cmdList->SetGraphicsRootConstantBufferView(
        static_cast<UINT>(RootSlot::PerFrameConstant),
        cbvAddress
    );

    // test
    //------------------------------------------------------------------------------------------------------------------
    const auto* height =m_resourceManager->GetTextureManager()->TryGetTexture(m_oceanHeight);
    if (!height || height->srv.index == UINT32_MAX) {
        throw std::logic_error("RenderSystem::BindGlobalRenderResource : 高度圖不存在或缺少 SRV");
    }

    cmdList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(RootSlot::OceanHeightTable),
        height->srv.gpu
    );
}

// Render流程 - 2 綁定裁切視窗
void RenderSystem::SetViewAndScissor() {
    auto* cmdList = m_queueManager->GetList(QueueType::Direct);

    //設定視野&裁切範圍
    cmdList->RSSetViewports(1, &screenViewport);
    cmdList->RSSetScissorRects(1, &scissorRect);
}

// Render流程 - 3 清除當前幀BackBuffer & 切換Barrier到繪製狀態
void RenderSystem::ClearBackBuffer(uint32_t currentFrame) {
    auto* cmdList = m_queueManager->GetList(QueueType::Direct);
    auto* pBackBuffer = m_d3d12Engine->GetBackBuffer();
    const auto dsvHandle = m_d3d12Engine->GetDsvHandle();

    //建立Barrier 更改SwapChian當前Buffer的狀態
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &barrier);

    //指定目前這幀的swapchain rtv
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_d3d12Engine->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_d3d12Engine->GetRtvDescriptorSize() * currentFrame;
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    //刷新畫面
    const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    cmdList->ClearRenderTargetView(
        rtvHandle, 
        clearColor, 
        0, 
        nullptr
    );
    //刷新dsv
    cmdList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );
}

// Render流程 - 4 Mesh Pass       
void RenderSystem::DrawMeshPass(const SceneRenderData& data) {
    auto* cmdList = m_queueManager->GetList(QueueType::Direct);
    auto* meshes = m_resourceManager->GetMeshManager();
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& batch : data.batches) {
        const std::size_t offset = batch.instanceOffset;
        const std::size_t count = batch.instanceCount;
        const std::size_t total = data.instanceDatas.size();

        // 若當前物件Instance數量為0 則跳過當前batch
        if (count == 0) {
            continue;
        }

        // 若當前物件Instance數量超過當前總Instance數量 表示傳入資料有誤 跳過
        if (offset > total || count > total - offset) {
            continue;
        }

        // 無效handle 缺少buffer 則跳過當前batch
        const auto* meshPtr = meshes->TryGetMesh(batch.mesh);
        if (!meshPtr) {
            continue; 
        }
        const auto& mesh = *meshPtr;

        cmdList->SetPipelineState(m_pipelineManager->GetPso(batch.pso));
        cmdList->SetGraphicsRoot32BitConstant(
            static_cast<UINT>(RootSlot::InstanceOffset),
            batch.instanceOffset,
            0
        );

        cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList->IASetIndexBuffer(&mesh.indexBufferView);
        cmdList->DrawIndexedInstanced(mesh.indexCount, batch.instanceCount, 0, 0, 0);
    }
}

// Render流程 - 5 切換Barrier到呈現狀態
void RenderSystem::TransitionBackBuffer(uint32_t currentFrame) {
    auto* cmdList = m_queueManager->GetList(QueueType::Direct);
    auto* pBackBuffer = m_d3d12Engine->GetBackBuffer();
    
    //修改Barrier 更改SwapChain當前Buffer的狀態
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
}



// Ocean Pass
void RenderSystem::InitOceanHeight() {
    auto* textures = m_resourceManager->GetTextureManager();

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = OceanHeightResolution;
    desc.Height = OceanHeightResolution;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    m_oceanHeight = textures->CreateTexture2D(
        "OceanHeight",
        desc,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        true
    );
}

void RenderSystem::PrepareOceanHeight(const OceanWaveParameters& parameters){
    auto* textures = m_resourceManager->GetTextureManager();
    auto* directList = m_queueManager->GetList(QueueType::Direct);

    const auto* height = textures->TryGetTexture(m_oceanHeight);
    if (!height || !height->hasUav) {
        throw std::logic_error("RenderSystem::PrepareOceanHeight : 高度圖不存在或缺少 UAV");
    }

    // 目前固定流程：每次開始與結束都處於 NON_PIXEL_SHADER_RESOURCE。
    textures->TransitionTexture(
        m_oceanHeight,
        directList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );

    directList->SetComputeRootSignature(
        m_d3d12Engine->GetRootsig()
    );

    directList->SetPipelineState(
        m_pipelineManager->GetOceanComputePso()
    );

    directList->SetComputeRootDescriptorTable(
        static_cast<UINT>(RootSlot::OceanHeightUav),
        height->uav.gpu
    );

    static_assert(
        sizeof(OceanWaveParameters) == 4 * sizeof(uint32_t),
        "OceanWaveParameters must match four root constants"
        );
    directList->SetComputeRoot32BitConstants(
        static_cast<UINT>(RootSlot::OceanWaveParameters),
        4,
        &parameters,
        0
    );

    // 目前 HLSL 的 numthreads 為 8 × 8。
    const uint32_t groupCount =(OceanHeightResolution + 7) / 8;
    directList->Dispatch(groupCount, groupCount, 1);

    textures->TransitionTexture(
        m_oceanHeight,
        directList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );
}