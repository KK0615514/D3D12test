#pragma once
#include "IScene.h"

#include "system/Random.h"
#include "system/RenderData.h"
#include "system/MeshData.h"

//後續拆掉IScene
class Game3DScene:public IScene {
public:
	Game3DScene(GameContext& ctx);
	~Game3DScene() = default;

	// 資源載入
	void SetMeshes(std::vector<MeshHandle> meshes) override;
	const SceneRenderData& GetRenderData() const override;
	void SetOceanMesh(MeshHandle handle) override;
	const OceanMeshData& GetOceanMeshData() const override;

	// 常規函式
	void Update() override;

	//test
	void UpdateCamera();			//歐拉角 之後改四元數								
	void MoveSystem();
	void SpwanAndMoveEnemy();
	void RotateCamera();

	void BuildOceanRenderData();	//test 海生成
	//test

private:
	OceanMeshData m_oceanMeshData;		//細分好的物件

	SceneRenderData m_renderData;		//每幀的繪畫資料

	entt::registry m_registry;			//EnTT核心

	entt::entity m_player;
	entt::entity m_camera;
	
private:
	//  test
	float m_cameraYaw = 0.0f;
	float m_cameraPitch = 0.0f;
	int m_lastMouseX = 0;
	int m_lastMouseY = 0;
	bool m_cameraDragging = false;
	//test
};