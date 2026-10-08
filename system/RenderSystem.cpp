#include "RenderSystem.h"

#include <cstddef>

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

    m_resourceManager = std::make_unique<ResourceManager>(m_device);
    m_resourceManager->Init();

	m_textureManager = std::make_unique<TextureManager>(m_device);
	m_textureManager->Init();

    //預設只有一種rootsig
    m_pipelineManager = std::make_unique<PipelineManager>(m_device, m_rootsig);
    m_pipelineManager->Init();

    InitializeViewPortAndScissorRect();
}

// 之後隨著視窗變形 或是想要啥鏡頭效果會需要提供 set這兩項的功能
void RenderSystem::InitializeViewPortAndScissorRect() {
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

// 很醜 要大修 現在不只texture 還有 mesh   
/*// 後續改成回傳 texture handle 給Scene 跟下面一樣
void RenderSystem::LoadTexture() {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();
    ID3D12GraphicsCommandList1* directList = m_queueManager->GetList(QueueType::Direct);
    ID3D12GraphicsCommandList1* copyList = m_queueManager->GetList(QueueType::Copy);

    m_queueManager->BeginCommandRecording(QueueType::Copy, currentFrame);

    //texture
    //m_textureManager->LoadTexture("asset/texture/poop.png", copyList);
    //m_textureManager->LoadTexture("asset/texture/waterball.png", copyList);
    //m_textureManager->LoadTexture("asset/texture/Suck.jpg", copyList);

    //mesh test
    m_resourceManager->Load2DMesh(copyList);
    const auto result = m_resourceManager->LoadMesh("asset/model/test.glb", copyList);

    m_queueManager->EndAndExecuteCommandRecording(QueueType::Copy, currentFrame);
    uint64_t waitForCopy = m_queueManager->Signal(QueueType::Copy);
    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    m_queueManager->QueueWait(QueueType::Direct, QueueType::Copy, waitForCopy);
    m_queueManager->BeginCommandRecording(QueueType::Direct, currentFrame);

    m_queueManager->EndAndExecuteCommandRecording(QueueType::Direct, currentFrame);

    m_queueManager->FlushCommandQueue(QueueType::Direct);

    m_textureManager->ClearUploadBuffer();
    m_resourceManager->ClearUploadBuffer();
}
*/

std::vector<MeshHandle> RenderSystem::LoadMeshes(const std::vector<std::string>& objects) {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();
    ID3D12GraphicsCommandList1* directList = m_queueManager->GetList(QueueType::Direct);
    ID3D12GraphicsCommandList1* copyList = m_queueManager->GetList(QueueType::Copy);
    std::vector<MeshLoadResult> results;  
    results.reserve(objects.size());

    std::vector<std::string> paths;
    // 把物件名稱擴充成指定路徑 
    for (auto& object : objects) {
        paths.emplace_back("asset/model/" + object +".glb");
    }

    m_queueManager->BeginCommandRecording(QueueType::Copy, currentFrame);
    for (auto path : paths) {
        results.emplace_back(m_resourceManager->LoadMesh(path.c_str(), copyList));
    }
    m_queueManager->EndAndExecuteCommandRecording(QueueType::Copy, currentFrame);
    uint64_t waitForCopy = m_queueManager->Signal(QueueType::Copy);
    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    m_queueManager->QueueWait(QueueType::Direct, QueueType::Copy, waitForCopy);
    m_queueManager->BeginCommandRecording(QueueType::Direct, currentFrame);
    for (const auto& result : results) {
        if (result.newlyCreated)
            m_resourceManager->TransitionMeshBuffer(result.handle, directList);
    }
    m_queueManager->EndAndExecuteCommandRecording(QueueType::Direct, currentFrame);

    // 寫死的全部flush 之後改成動態檢測狀態 任務完成的才沖掉
    m_queueManager->FlushCommandQueue(QueueType::Direct);

    m_textureManager->ClearUploadBuffer();
    m_resourceManager->ClearUploadBuffer();

    std::vector<MeshHandle> handles;
    handles.reserve(results.size());
    for (const auto& result : results) {
        handles.push_back(result.handle);
    }
    return handles;
}

// 現在每幀fence等待是在這裡 之後要修改 不然太醜
void RenderSystem::Update(const SceneRenderData& data) {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();

    // 一幀的開始 啟用direct queue
    m_queueManager->WaitFrameReady(QueueType::Direct, currentFrame);

    // 等待前一幀Fence完成後才更新CB SB    避免極大量物件時cmdlist還在讀取資料 然後Buffer就更新了
    // 我猜測 之前80000大便會閃爍就是Buffer還沒讀取畫完就更新下一幀的資料 因此造成閃爍
    m_resourceManager->Update(data, currentFrame);       
}

void RenderSystem::Render(const SceneRenderData& data) {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();

    m_queueManager->BeginCommandRecording(QueueType::Direct, currentFrame);

    BindGlobalRenderResource(currentFrame);
    SetViewAndScissor();
    ClearBackBuffer(currentFrame);
    DrawMeshPass(data);
    TransitionBackBuffer(currentFrame);

    m_queueManager->EndAndExecuteCommandRecording(QueueType::Direct, currentFrame);
    m_queueManager->SignalCurrentFrame(QueueType::Direct, currentFrame);
    m_d3d12Engine->UpdateSwapChainBackBufferIndex();    // 呈現畫面&直接切換frame 不在這裡等signal 等下次輪換到這個frame時在另一個函式確認完成與否
}

// Render流程 - 1 綁定各種資源
void RenderSystem::BindGlobalRenderResource(uint32_t currentFrame) {
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);

    //指定texture 的 srv 
    cmdList->SetDescriptorHeaps(1, m_textureManager->m_srvHeap.GetAddressOf());

    //每幀綁定root signature
    cmdList->SetGraphicsRootSignature(m_d3d12Engine->GetRootsig());

    //設定tex srv as descriptor table
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = m_textureManager->m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    cmdList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(RootSlot::TextureTable),
        textureSrvHandle
    );
    //設定sb srv as root descriptor
    D3D12_GPU_VIRTUAL_ADDRESS sbAddress = m_resourceManager->m_structureBuffer[currentFrame]->GetGPUVirtualAddress();
    cmdList->SetGraphicsRootShaderResourceView(
        static_cast<UINT>(RootSlot::InstanceBuffer),
        sbAddress
    );
    //設定cbv as root descriptor
    D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = m_resourceManager->m_constantBuffer[currentFrame][0]->GetGPUVirtualAddress();
    cmdList->SetGraphicsRootConstantBufferView(
        static_cast<UINT>(RootSlot::PerFrameConstant),
        cbvAddress
    );
}

// Render流程 - 2 綁定裁切視窗
void RenderSystem::SetViewAndScissor() {
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);

    //設定視野&裁切範圍
    cmdList->RSSetViewports(1, &screenViewport);
    cmdList->RSSetScissorRects(1, &scissorRect);
}

// Render流程 - 3 清除當前幀BackBuffer & 切換Barrier到繪製狀態
void RenderSystem::ClearBackBuffer(uint32_t currentFrame) {
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);
    ID3D12Resource* pBackBuffer = m_d3d12Engine->GetBackBuffer();
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
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);

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
        const auto* meshPtr = m_resourceManager->TryGetMesh(batch.mesh);
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
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);
    ID3D12Resource* pBackBuffer = m_d3d12Engine->GetBackBuffer();
    
    //修改Barrier 更改SwapChain當前Buffer的狀態
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
}

// 主迴圈最後釋放資源
void RenderSystem::Shutdown() {
    m_queueManager->FlushCommandQueue(QueueType::Copy);
    m_queueManager->FlushCommandQueue(QueueType::Compute);
    m_queueManager->FlushCommandQueue(QueueType::Direct);
}