#pragma once
#include <cmath>	//用abs絕對值
#include "entt/entt.hpp"

#include <DirectXMath.h>

//刪除
#include "IScene.h"

struct Transform{

};


//後續拆掉IScene
class Game3DScene:public IScene {
public:
	Game3DScene();
	~Game3DScene() = default;

	void Update();

	entt::registry m_Registry;

//以下刪除
public:		
	void Updateaaa() override;
	std::unique_ptr<IScene> ChangeScene() override;
	uint32_t PackInstanceData() override;

private:

};