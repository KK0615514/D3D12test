#pragma once

#include "system/D3D12Engine.h"
#include "system/QueueManager.h"
#include "system/ResourceManager.h"
#include "system/PipelineManager.h"

#include "system/CommonUtils.h"

//修改???
#include "system/TextureManager.h"

enum class RootSlot: UINT{
	TextureTable = 0,
	InstanceBuffer = 1,
	PerFrameConstant = 2,
	InstanceOffset = 3
};	

class RenderSystem
{
public:
	RenderSystem() = default;
	~RenderSystem() = default;

	RenderSystem (const RenderSystem&) = delete;
	RenderSystem(RenderSystem&&) = delete;
	RenderSystem operator=(const RenderSystem&) = delete;
	RenderSystem operator=(RenderSystem&&) = delete;

	void Init(HWND hwnd);
	void Update(const SceneRenderData& data);
	void Render(const SceneRenderData& data);
	void Shutdown();

	MeshHandle LoadOceanMesh(const OceanMeshData& meshData);
	std::vector<MeshHandle> LoadMeshes(const std::vector<std::string>& objects);

private:
	 std::unique_ptr<D3D12Engine>			m_d3d12Engine;
	 std::unique_ptr<QueueManager>			m_queueManager;
	 std::unique_ptr<ResourceManager>		m_resourceManager;
	 std::unique_ptr<PipelineManager>		m_pipelineManager;

	 //後續新增resource manager
	 std::unique_ptr<TextureManager>	m_textureManager;

	 //視窗與裁切規格	如果要小地圖等功能可多個
	 D3D12_VIEWPORT						screenViewport{};
	 D3D12_RECT							scissorRect{};

	 void InitializeViewPortAndScissorRect();
	 void LoadTexture();



	 void ClearBackBuffer(uint32_t currentFrame);
	 void BindGlobalRenderResource(uint32_t currentFrame);
	 void SetViewAndScissor();
	 void DrawTexturePass(const SceneRenderData& data);

	 //test
	 void DrawMeshPass(const SceneRenderData& data);
	 void TransitionBackBuffer(uint32_t currentFrame);
};