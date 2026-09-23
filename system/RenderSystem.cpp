#include "RenderSystem.h"

void RenderSystem::Init(HWND hwnd) {
    m_d3d12Engine = std::make_unique<D3D12Engine>();
    m_d3d12Engine->InitDevice();      //queue需要device swapchain需要queue 逼不得已

    m_queueManager = std::make_unique<QueueManager>(m_d3d12Engine->m_device.Get());
    m_queueManager->Init();
    m_d3d12Engine->InitSwapChain(hwnd, m_queueManager->m_commandQueue.Get());
    m_d3d12Engine->InitRootSig();

    m_resourceManager = std::make_unique<ResourceManager>(m_d3d12Engine->m_device.Get());
    m_resourceManager->Init();

	m_textureManager = std::make_unique<TextureManager>(m_d3d12Engine->m_device.Get());
	m_textureManager->Init();

    m_pipelineManager = std::make_unique<PipelineManager>(m_d3d12Engine->m_device.Get(), m_d3d12Engine->m_rootSignature.Get());
    m_pipelineManager->Init();

    InitializeViewPortAndScissorRect();
    LoadTexture();
}

void RenderSystem::InitializeViewPortAndScissorRect() {
    screenViewport.TopLeftX = 0.0f;
    screenViewport.TopLeftY = 0.0f;
    screenViewport.Width = 1280.0f;
    screenViewport.Height = 720.0f;
    screenViewport.MinDepth = 0.0f;
    screenViewport.MaxDepth = 1.0f;

    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = 1280;
    scissorRect.bottom = 720;
}

// 很醜 要大修
void RenderSystem::LoadTexture() {
    uint32_t currentFrame = m_d3d12Engine->m_frameIndex;
    auto& textureLoadQueue = m_queueManager->m_commandList;

    m_queueManager->m_commandAllocator[currentFrame]->Reset();
    m_queueManager->m_commandList->Reset(m_queueManager->m_commandAllocator[currentFrame].Get(), nullptr);

    m_textureManager->LoadTexture("asset/texture/pickle.png", textureLoadQueue.Get());         //0
    m_textureManager->LoadTexture("asset/texture/poop.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/waterball.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/crosshair.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/yUI.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/rUI.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/START.png", textureLoadQueue.Get());          //6
    m_textureManager->LoadTexture("asset/font/eng.png", textureLoadQueue.Get());
    m_textureManager->LoadTexture("asset/texture/Suck.jpg", textureLoadQueue.Get());

    //test
    m_textureMesh = m_resourceManager->Load2DMesh(m_queueManager->m_commandList.Get());
    m_testMesh = m_resourceManager->LoadMesh("asset/model/test.glb", textureLoadQueue.Get());

    m_queueManager->m_commandList->Close();
    ID3D12CommandList* cmds[] = { textureLoadQueue.Get() };
    m_queueManager->m_commandQueue->ExecuteCommandLists(1, cmds);

    m_queueManager->FlushCommandQueue();
    m_textureManager->ClearUploadBuffer();

    //test
    m_resourceManager->ClearUploadBuffer();
}

// 流程有待簡化
void RenderSystem::Render() {
    uint32_t currentFrame = m_d3d12Engine->m_frameIndex;
    auto& pBackBuffer = m_d3d12Engine->m_renderTargets[currentFrame];

    //一幀的開始
    m_queueManager->WaitFrameReady(currentFrame);
    m_queueManager->BeginCommandRecording(currentFrame, pBackBuffer.Get());

    ClearBackBuffer(currentFrame);
    BindGlobalRenderResource(currentFrame);
    SetViewAndScissor();
    DrawTexturePass();

    //test
    DrawMeshPass();

    m_queueManager->EndAndExecuteCommandRecording(currentFrame, pBackBuffer.Get());
    m_queueManager->SignalCurrentFrame(currentFrame);
    m_d3d12Engine->UpdateSwapChainBackBufferIndex();    // 呈現畫面&直接切換frame 不在這裡等signal 等下次輪換到這個frame時在另一個函式確認完成與否
}

// Render流程 - 1
void RenderSystem::ClearBackBuffer(uint32_t currentFrame) {
    auto& cmdList = m_queueManager->m_commandList;

    //指定目前這幀的swapchain rtv
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_d3d12Engine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_d3d12Engine->m_rtvDescriptorSize * currentFrame;
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    //刷新畫面
    const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

// Render流程 - 2
void RenderSystem::BindGlobalRenderResource(uint32_t currentFrame) {
    auto& cmdList = m_queueManager->m_commandList;

    //指定texture 的 srv 
    cmdList->SetDescriptorHeaps(1, m_textureManager->m_srvHeap.GetAddressOf());

    //每幀綁定root signature
    cmdList->SetGraphicsRootSignature(m_d3d12Engine->m_rootSignature.Get());

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

// Render流程 - 3
void RenderSystem::SetViewAndScissor() {
    auto& cmdList = m_queueManager->m_commandList;

    //設定視野&裁切範圍
    cmdList->RSSetViewports(1, &screenViewport);
    cmdList->RSSetScissorRects(1, &scissorRect);
}

// Render流程 - 4
void RenderSystem::DrawTexturePass()
{
    if (!m_textureMesh) return;

    const auto& mesh = m_resourceManager->GetMesh(*m_textureMesh);
    auto& cmdList = m_queueManager->m_commandList;

    //綁定PSO
    cmdList->SetPipelineState(m_pipelineManager->GetPso(PsoType::TextureLoader));

    //綁定設定好的VB IB
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
    cmdList->IASetIndexBuffer(&mesh.indexBufferView);

    cmdList->DrawIndexedInstanced(mesh.indexCount, InstanceCounts, 0, 0, 0);
}

//test
void RenderSystem::DrawMeshPass() {
    if (!m_testMesh) return;

    const auto& mesh = m_resourceManager->GetMesh(*m_testMesh);
    auto* cmdList = m_queueManager->m_commandList.Get();

    cmdList->SetPipelineState(m_pipelineManager->GetPso(PsoType::MeshDebug));
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
    cmdList->IASetIndexBuffer(&mesh.indexBufferView);
    cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

// 臨時用的 改成0複製
void RenderSystem::Update(IScene& currentScene) {
    uint32_t currentFrame = m_d3d12Engine->m_frameIndex;

    m_resourceManager->Update(currentScene, currentFrame);       //更新CB SB
    InstanceCounts = currentScene.instanceCounts;
}

void RenderSystem::Shutdown() {

}
