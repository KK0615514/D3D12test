#include "ResultScene.h"
#include <iostream>

ResultScene::ResultScene() {
	m_player.pos = { 640,480,0 };
	m_player.scale = { 75,75,1 };
	m_player.textureID = 999;

	m_crosshair.pos = { 640,160,0 };
	m_crosshair.textureID = 999;

	m_score = finalscore;

	UI UIA{};
	UIA.pos = { 640,360,0 };
	UIA.scale = { 150,150,0 };
	UIA.textureID = 8;

	m_UI.push_back(UIA);

	currentScene = SceneType::Result;
}

void ResultScene::Update() {
}

std::unique_ptr<IScene> ResultScene::ChangeScene() {
	if (InputManager::IsKeyDown(13)) {
		auto nextScene = std::make_unique<MenuScene>();
		return nextScene;
	}
	return nullptr;
}

uint32_t ResultScene::PackInstanceData() {
	uint32_t objectCounts{};

	objectCounts += 3;					//player + crosshair + UI
	objectCounts += m_enemys.size();	//enemy
	objectCounts += m_bullets.size();	//bullet

	return objectCounts;
}