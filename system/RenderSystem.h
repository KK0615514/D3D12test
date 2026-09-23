#pragma once

#include "D3D12Engine.h"
#include "QueueManager.h"
#include "ResourceManager.h"

#include "TextureManager.h"
#include "PipelineManager.h"

//test
#include <optional>

enum class RootSlot : uint16_t {
	TextureTable = 0,
	InstanceBuffer = 1,
	PerFrameConstant = 2
};

class RenderSystem
{
public:
	RenderSystem (const RenderSystem&) = delete;
	RenderSystem(RenderSystem&&) = delete;
	RenderSystem operator=(const RenderSystem&) = delete;
	RenderSystem operator=(RenderSystem&&) = delete;

	//後面改成傳值
	uint32_t InstanceCounts{ 0 };

	void Init(HWND hwnd);
	void Render();
	void Update(IScene& currentScene);
	void Shutdown();

	static RenderSystem& GetInstance() {
		static RenderSystem instance;
		return instance;
	};

private:
	 std::unique_ptr<D3D12Engine>			m_d3d12Engine;
	 std::unique_ptr<QueueManager>			m_queueManager;
	 std::unique_ptr<ResourceManager>		m_resourceManager;
	 std::unique_ptr<PipelineManager>		m_pipelineManager;

	 //後續新增resource manager
	 std::unique_ptr<TextureManager>	m_textureManager;

	 //test
	 std::optional<MeshHandle> m_textureMesh;
	 std::optional<MeshHandle> m_testMesh;

	 //視窗與裁切規格	如果要小地圖等功能可多個
	 D3D12_VIEWPORT						screenViewport{};
	 D3D12_RECT							scissorRect{};

	 void ClearBackBuffer(uint32_t currentFrame);
	 void BindGlobalRenderResource(uint32_t currentFrame);
	 void SetViewAndScissor();
	 void DrawTexturePass();

	 //test
	 void DrawMeshPass();
 
	 void InitializeViewPortAndScissorRect();
	 void LoadTexture();

	 RenderSystem() = default;
	 ~RenderSystem() = default;
};