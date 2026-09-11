#pragma once

#include "IScene.h"
#include "MenuScene.h"

class ResultScene :public IScene
{
public:
	ResultScene();
	~ResultScene() = default;

	void Update() override;
	std::unique_ptr<IScene> ChangeScene() override;
	uint32_t PackInstanceData() override;

private:
};