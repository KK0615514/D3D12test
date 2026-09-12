#pragma once

#include "IScene.h"
#include "GameScene.h"

class MenuScene :public IScene
{
public:
	MenuScene();
	~MenuScene() = default;

	void Update() override;
	std::unique_ptr<IScene> ChangeScene() override;
	uint32_t PackInstanceData() override;

private:
	
	void SpawnEnemy();
	void Move();

};