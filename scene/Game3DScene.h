#pragma once
#include <DirectXMath.h>

#include "IScene.h"
#include "system/CommonUtils.h"

//後續拆掉IScene
class Game3DScene:public IScene {
public:
	Game3DScene();
	~Game3DScene() = default;

	void Update() override;


private:
	entt::registry m_registry;

	entt::entity m_camera;
	entt::entity m_player;
};