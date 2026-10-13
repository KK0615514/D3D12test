#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "system/D3D12Engine.h"
#include "system/QueueManager.h"
#include "system/ResourceManager.h"
#include "system/PipelineManager.h"

enum class RootSlot: UINT{
	TextureTable = 0,
	InstanceBuffer = 1,
	PerFrameConstant = 2,
	InstanceOffset = 3,
	OceanHeightTable = 4,		// test 測試用的
	OceanHeightUav = 5,			// test 測試用的
	OceanWaveParameters = 6		// test 測試用的
};	

class RenderSystem
{
public:
	RenderSystem() = default;
	~RenderSystem() = default;

	RenderSystem(const RenderSystem&) = delete;
	RenderSystem(RenderSystem&&) = delete;
	RenderSystem& operator=(const RenderSystem&) = delete;
	RenderSystem& operator=(RenderSystem&&) = delete;

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

	 //視窗與裁切規格	如果要小地圖等功能可多個
	 D3D12_VIEWPORT						screenViewport{};
	 D3D12_RECT							scissorRect{};

	 void InitViewPortAndScissorRect();

	 void ClearBackBuffer(uint32_t currentFrame);
	 void BindGlobalRenderResource(uint32_t currentFrame);
	 void SetViewAndScissor();
	 void DrawMeshPass(const SceneRenderData& data);
	 void TransitionBackBuffer(uint32_t currentFrame);

private:	// Ocean Pass
	static constexpr uint32_t OceanHeightResolution = 256;
	TextureHandle m_oceanHeight{ UINT32_MAX };
	void InitOceanHeight();
	void PrepareOceanHeight(const OceanWaveParameters& parameters);
};