#pragma once
#include <DirectXMath.h>

#include "IScene.h"

#include "system/CommonUtils.h"
#include "system/Random.h"
#include "system/RenderData.h"

//後續拆掉IScene
class Game3DScene:public IScene {
public:
	Game3DScene();
	~Game3DScene() = default;

	void Update() override;

	const SceneRenderData& GetRenderData() const override;

private:
	SceneRenderData m_renderData;
	entt::registry m_registry;

	entt::entity m_camera;
	entt::entity m_player;

	void moveSystem();
};