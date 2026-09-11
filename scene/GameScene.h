#pragma once
#include <cmath>	//用abs絕對值

#include "IScene.h"
#include "ResultScene.h"

#include "../system/Timer.h"

//垃圾
#include "../gameobject/Audio.h"

class GameScene:public IScene
{
public:
	GameScene();
	~GameScene() = default;

	void Update() override;
	std::unique_ptr<IScene> ChangeScene() override;
	uint32_t PackInstanceData() override;

private:

	float playerAtkCd{};
	float enemySpawnCd{};

	void SpawnEnemy();
	void Move();
	void PlayerAttack();
	void OnCollide();	//個物件的collisiontype用 可能不需要了
};