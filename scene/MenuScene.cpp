#include "MenuScene.h"

MenuScene::MenuScene() {
	m_player.pos = { 640,480,0 };
	m_player.scale = { 75,75,1 };
	m_player.textureID = 999;
		
	m_crosshair.pos = { 640,160,0 };
	m_crosshair.textureID = 999;

	UI startbutton{};
	startbutton.pos = { 640,360,0 };
	startbutton.textureID = 6;
	m_UI.emplace_back(startbutton);
	
	m_enemys.reserve(1000);

	currentScene = SceneType::Menu;
}

void MenuScene::Update() {
	finalscore = 0;
	SpawnEnemy();
	Move();
}

std::unique_ptr<IScene> MenuScene::ChangeScene() {
	
	if (InputManager::IsKeyDown(13)) {
		auto nextScene = std::make_unique<GameScene>();
		return nextScene;
	}

	return nullptr;
}

uint32_t MenuScene::PackInstanceData(){
	uint32_t objectCounts{};

	objectCounts += 3;					//player + crosshair + UI
	objectCounts += m_enemys.size();	//enemy
	//objectCounts += m_bullets.size();	//bullet

	return objectCounts;
}

void MenuScene::SpawnEnemy() {
	Enemy newEnemy;
	float r = Xoshiro128::Random();

	newEnemy.HP = 3;
	newEnemy.textureID = 1;
	newEnemy.speed = 200.0f;
	if (0.0f <= r && r < 0.25f) {
		newEnemy.pos = { 1280.0f * Xoshiro128::Random() , 0.0f , 0.0f };
		newEnemy.angle = 90.0f;
	}
	else if (0.25f <= r && r < 0.5f) {
		newEnemy.pos = { 1280.0f * Xoshiro128::Random() , 720.0f , 0.0f };
		newEnemy.angle = 270.0f;
	}
	else if (0.5f <= r && r < 0.75f) {
		newEnemy.pos = { 1280.0f ,720.0f * Xoshiro128::Random() , 0.0f };
		newEnemy.angle = 180.0f;
	}
	else {
		newEnemy.pos = { 0.0f ,720.0f * Xoshiro128::Random() , 0.0f };
		newEnemy.angle = 0.0f;
	}

	float radians = DirectX::XMConvertToRadians(newEnemy.angle);
	float sinValue, cosValue;
	DirectX::XMScalarSinCos(&sinValue, &cosValue, radians);
	newEnemy.dir.y = sinValue;
	newEnemy.dir.x = cosValue;

	m_enemys.push_back(newEnemy);
}

void MenuScene::Move() {
	float delta = Timer::GetInstance().GetDeltaTime();

	for (size_t i = 0; i < m_enemys.size(); i++) {
		auto& enemy = m_enemys[i];
		enemy.pos.x += enemy.dir.x * enemy.speed * delta;
		enemy.pos.y += enemy.dir.y * enemy.speed * delta;

		if (enemy.pos.x < 0.0f || enemy.pos.x> 1280.0f || enemy.pos.y < 0.0f || enemy.pos.y > 720.0f)
		{
			std::swap(m_enemys[i], m_enemys.back());
			m_enemys.pop_back();
		}
	}
}