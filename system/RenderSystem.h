#pragma once

#include "TextureManager.h"
#include "RenderManager.h"
#include "D3D12Engine.h"
#include "QueueManager.h"

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
	 std::unique_ptr<D3D12Engine>		m_d3d12Engine;
	 std::unique_ptr<QueueManager>		m_queueManager;

	 //後續增加 psoManager
	 std::unique_ptr<RenderManager>		m_renderManager;

	 //後續新增buffer manager
	 std::unique_ptr<TextureManager>	m_textureManager;

	 //視窗與裁切規格	如果要小地圖等功能可多個
	 D3D12_VIEWPORT						screenViewport{};
	 D3D12_RECT							scissorRect{};

	 void SetViewPortAndScissorRect();
	 void LoadTexture();

	 RenderSystem() = default;
	 ~RenderSystem() = default;
};