#include "RenderSystem.h"

#include "system/RenderData.h"

void RenderSystem::Init(HWND hwnd) {
    m_d3d12Engine = std::make_unique<D3D12Engine>();
    m_d3d12Engine->InitDevice();      //queue需要device swapchain需要queue 逼不得已
    ID3D12Device4* m_device = m_d3d12Engine->GetDevice();

    m_queueManager = std::make_unique<QueueManager>(m_device);
    m_queueManager->Init();
    m_d3d12Engine->InitSwapChain(hwnd, m_queueManager->GetQueue(QueueType::Direct));
    m_d3d12Engine->InitRootSig();
    ID3D12RootSignature* m_rootsig = m_d3d12Engine->GetRootsig();

    m_resourceManager = std::make_unique<ResourceManager>(m_device);
    m_resourceManager->Init();

	m_textureManager = std::make_unique<TextureManager>(m_device);
	m_textureManager->Init();

    m_pipelineManager = std::make_unique<PipelineManager>(m_device, m_rootsig);
    m_pipelineManager->Init();

    InitializeViewPortAndScissorRect();
    //LoadTexture();
}

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

// test 看情況取代上面 
// 上面目前沒用
std::vector<MeshHandle> RenderSystem::LoadMeshes(const std::vector<std::string>& paths) {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();
    ID3D12GraphicsCommandList1* directList = m_queueManager->GetList(QueueType::Direct);
    ID3D12GraphicsCommandList1* copyList = m_queueManager->GetList(QueueType::Copy);
    std::vector<MeshLoadResult> results;

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

    //一幀的開始 啟用direct queue
    m_queueManager->WaitFrameReady(QueueType::Direct, currentFrame);
    //目前Iscene 只持有一組vector + InstanceCount
    m_resourceManager->Update(data, currentFrame);       //更新CB SB
}

// 流程有待簡化
void RenderSystem::Render(const SceneRenderData& data) {
    uint32_t currentFrame = m_d3d12Engine->GetFrameIndex();

    m_queueManager->BeginCommandRecording(QueueType::Direct, currentFrame);

    BindGlobalRenderResource(currentFrame);
    SetViewAndScissor();
    
    ClearBackBuffer(currentFrame);
    //目前無2D物件
    //DrawTexturePass();
    //test
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
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    //刷新畫面
    const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

// Render流程 - 4 Pass            有待修改 現在count 是亂填的 等到handle做完 才能完整
void RenderSystem::DrawTexturePass(const SceneRenderData& data){
    MeshHandle texture{ 0 };
    const auto& mesh = m_resourceManager->GetMesh(texture);
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);

    //綁定PSO
    cmdList->SetPipelineState(m_pipelineManager->GetPso(PsoType::TextureLoader));
    //綁定設定好的VB IB
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
    cmdList->IASetIndexBuffer(&mesh.indexBufferView);
    //實際作畫
    cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

// Render流程 - 4 test Pass       有待修改 現在count 是亂填的 等到handle做完 才能完整
void RenderSystem::DrawMeshPass(const SceneRenderData& data) {
    ID3D12GraphicsCommandList1* cmdList = m_queueManager->GetList(QueueType::Direct);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const auto& batch : data.batches) {
        //改
        const auto& mesh = m_resourceManager->GetMesh(batch.mesh);
        cmdList->SetPipelineState(m_pipelineManager->GetPso(batch.pso));

        cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList->IASetIndexBuffer(&mesh.indexBufferView);
        cmdList->DrawIndexedInstanced(mesh.indexCount, batch.instanceCount, 0, 0, batch.instanceOffset);
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



void RenderSystem::Shutdown() {

}


/*
handle存取做完 
    動loadtexture 改名字 改流程 
    動兩個pass 

單子系統整體弄完 回頭弄update 跟render 不然現在很醜
*/