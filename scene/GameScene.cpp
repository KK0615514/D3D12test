#include "GameScene.h"
#include <iostream>

GameScene::GameScene(){
	InstanceDatas.reserve(10000);

	m_player.pos = {640,480,0};
	m_player.scale = { 75,75,1 };
	m_player.textureID = 0;
	m_player.moveX = 350.0f;
	m_player.moveY = 350.0f;
	m_player.invincibleCD = 0.5f;
	m_player.HP = 3;
	m_player.atk = 1;

	m_crosshair.pos = { 640,160,0 };
	m_crosshair.textureID = 3;
	m_crosshair.moveX = 650.0f;
	m_crosshair.moveY = 650.0f;

	//m_UI.pos = { 0,0,0 };
	//m_UI.textureID = 999;

	m_enemys.reserve(1000);
	m_bullets.reserve(1000);
	SimpleAudio::Get().PlayBGM(L"C:/Ecs Project/out/build/x64-Debug/asset/bgm/custom_standard.wav");

	currentScene = SceneType::Game;
}

//IScene
void GameScene::Update() {
	float delta = Timer::GetInstance().GetDeltaTime();

	Move();
	//UIButton();
	
	//寫很爛的player 無敵
	if (m_player.isInvincibleCD)
	{
		m_player.invincibleTimer += delta;

		if (m_player.invincibleTimer >= m_player.invincibleCD)
		{
			m_player.isInvincibleCD = false;
			m_player.invincibleTimer = 0.0f;
		}
	}
	//痛苦碰撞
	OnCollide();

	enemySpawnCd += delta;
	if (enemySpawnCd > 0.20f) {

		for(int i =0;i<300;i++)	SpawnEnemy();
		enemySpawnCd -= 0.20f;
	}

	playerAtkCd += delta;
	if (playerAtkCd > 0.03f)
	{
		PlayerAttack();
		playerAtkCd -= 0.03f;
	}

	//std::cout << m_enemys.size() << std::endl;

	PackInstanceData();
}

//IScene
std::unique_ptr<IScene> GameScene::ChangeScene() {
	if (m_player.HP <= 0){
		auto nextScene = std::make_unique<ResultScene>();
		return nextScene;
	}
	return nullptr;
}

void GameScene::SpawnEnemy() {
	Enemy newEnemy;
	float r = Xoshiro128::Random();

	newEnemy.HP = 3;
	newEnemy.textureID = 1;
	newEnemy.speed = 200.0f;
	if (0.0f <= r && r < 0.25f){
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

void GameScene::Move() {
	float delta = Timer::GetInstance().GetDeltaTime();

	//player move
	if (InputManager::IsKeyHeld('W')) {
		if(m_player.pos.y >= 0.0f)	m_player.pos.y -= m_player.moveY * delta;
	}
	if (InputManager::IsKeyHeld('A')) {
		if (m_player.pos.x >= 0.0f) m_player.pos.x -= m_player.moveX * delta;
	}
	if (InputManager::IsKeyHeld('S')) {
		if (m_player.pos.y <= 720.0f) m_player.pos.y += m_player.moveY * delta;
	}
	if (InputManager::IsKeyHeld('D')) {
		if (m_player.pos.x <= 1280.0f) m_player.pos.x += m_player.moveX * delta;
	}

	//crosshair
	if (InputManager::IsKeyHeld(0x26)) {
		if (m_crosshair.pos.y >= 0.0f)m_crosshair.pos.y -= m_crosshair.moveY * delta;
	}
	if (InputManager::IsKeyHeld(0x25)) {
		if(m_crosshair.pos.x >= 0.0f) m_crosshair.pos.x -= m_crosshair.moveX * delta;
	}
	if (InputManager::IsKeyHeld(0x28)) {
		if (m_crosshair.pos.y <= 720.0f) m_crosshair.pos.y += m_crosshair.moveY * delta;
	}
	if (InputManager::IsKeyHeld(0x27)) {
		if(m_crosshair.pos.x <= 1280.0f) m_crosshair.pos.x += m_crosshair.moveX * delta;
	}

	//enemy move
	for (size_t i = 0; i < m_enemys.size(); i++){
		auto& enemy = m_enemys[i];
		enemy.pos.x += enemy.dir.x * enemy.speed * delta;
		enemy.pos.y += enemy.dir.y * enemy.speed * delta;

		if (enemy.pos.x < 0.0f || enemy.pos.x> 1280.0f|| enemy.pos.y < 0.0f|| enemy.pos.y > 720.0f)
		{
			std::swap(m_enemys[i], m_enemys.back());
			m_enemys.pop_back();
		}
	}

	//bullet move
	for (size_t i = 0; i < m_bullets.size(); i++) {
		auto& bullet = m_bullets[i];
		bullet.pos.x += bullet.dir.x * bullet.speed * delta;
		bullet.pos.y += bullet.dir.y * bullet.speed * delta;

		if (bullet.pos.x < 0.0f || bullet.pos.x> 1280.0f || bullet.pos.y < 0.0f || bullet.pos.y > 720.0f)
		{
			std::swap(m_bullets[i], m_bullets.back());
			m_bullets.pop_back();
		}
	}
}

void GameScene::PlayerAttack() {
	
	Bullet newBullet;

	newBullet.atk = m_player.atk;
	newBullet.pos.x = m_player.pos.x;
	newBullet.pos.y = m_player.pos.y;
	newBullet.textureID = 2;
	newBullet.speed = 1500.0f;

	DirectX::XMVECTOR vA = DirectX::XMLoadFloat3(&m_player.pos);
	DirectX::XMVECTOR vB = DirectX::XMLoadFloat3(&m_crosshair.pos);
	DirectX::XMVECTOR vDir = DirectX::XMVectorSubtract(vB,vA);
	DirectX::XMVECTOR vDirNormalize = DirectX::XMVector3Normalize(vDir);

	newBullet.dir.x = DirectX::XMVectorGetX(vDirNormalize);//cos
	newBullet.dir.y = DirectX::XMVectorGetY(vDirNormalize);//sin

	m_bullets.push_back(newBullet);
}

//沒畫格子 很痛苦的硬要碰撞檢測
//223可刪除
void GameScene::OnCollide() {
	
	//玩家 & 敵人
	for (auto& enemy : m_enemys) {
		float absX = std::abs(m_player.pos.x - enemy.pos.x);
		float absY = std::abs(m_player.pos.y - enemy.pos.y);

		float minX = (m_player.scale.x + enemy.scale.x -50) / 2;
		float minY = (m_player.scale.y + enemy.scale.y -50) / 2;

		if (absX < minX && absY < minY) {
			if(!m_player.isInvincibleCD)
			{
				m_player.HP -= 1;
				//std::cout << m_player.HP << std::endl;
				m_player.isInvincibleCD = true;
			}
		}
	}

	//子彈 & 敵人
	for (size_t i = 0; i < m_enemys.size(); ) {
		bool enemy_dead = false;
		for (size_t j = 0; j < m_bullets.size(); ) {
			auto& enemy = m_enemys[i];
			auto& bullet = m_bullets[j];
			float absX = std::abs(bullet.pos.x - enemy.pos.x);
			float absY = std::abs(bullet.pos.y - enemy.pos.y);

			float minX = (bullet.scale.x + enemy.scale.x -65)  / 2;
			float minY = (bullet.scale.y + enemy.scale.y -65)  / 2;

			if (absX < minX && absY < minY) {
				enemy.HP -= bullet.atk;
				std::swap(m_bullets[j], m_bullets.back());
				m_bullets.pop_back();

				if (enemy.HP <= 0) { 
					enemy_dead = true;
					break; 
				}
				continue; 
			}
			j++; 
		}
		if (enemy_dead) {
			//+分
			m_score++;
			//SimpleAudio::Get().Play(L"C:/Ecs Project/out/build/x64-Debug/asset/se/fart.wav");

			std::swap(m_enemys[i], m_enemys.back());
			m_enemys.pop_back();
		}
		else i++;
		
	}
}

//IScene
uint32_t GameScene::PackInstanceData() {
	uint32_t objectCounts{};
	InstanceDatas.clear();

	for (const auto& object : m_enemys) {
		DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&object.scale);
		DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

		DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&object.pos);
		DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

		// S縮放 * R旋轉 * T平移 這裡沒有旋轉
		DirectX::XMMATRIX mTransform = mScale * mTranslation;

		//轉置 C++ row major HLSL column major
		DirectX::XMMATRIX mTranspose = DirectX::XMMatrixTranspose(mTransform);

		InstanceData enemyInstance;
		DirectX::XMStoreFloat4x4(&enemyInstance.Transform, mTranspose);
		enemyInstance.TextureIndex = object.textureID;
		enemyInstance.Padding[0] = 0;
		enemyInstance.Padding[1] = 0;
		enemyInstance.Padding[2] = 0;

		InstanceDatas.emplace_back(enemyInstance);
	}

	for (const auto& object : m_bullets) {
		DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&object.scale);
		DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

		DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&object.pos);
		DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

		// S縮放 * R旋轉 * T平移 這裡沒有旋轉
		DirectX::XMMATRIX mTransform = mScale * mTranslation;

		//轉置 C++ row major HLSL column major
		DirectX::XMMATRIX mTranspose = DirectX::XMMatrixTranspose(mTransform);

		InstanceData bulletInstance;
		DirectX::XMStoreFloat4x4(&bulletInstance.Transform, mTranspose);
		bulletInstance.TextureIndex = object.textureID;
		bulletInstance.Padding[0] = 0;
		bulletInstance.Padding[1] = 0;
		bulletInstance.Padding[2] = 0;

		InstanceDatas.emplace_back(bulletInstance);
	}

	{
		DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&m_player.scale);
		DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

		DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&m_player.pos);
		DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

		// S縮放 * R旋轉 * T平移 這裡沒有旋轉
		DirectX::XMMATRIX mTransform = mScale * mTranslation;

		//轉置 C++ row major HLSL column major
		DirectX::XMMATRIX mTranspose = DirectX::XMMatrixTranspose(mTransform);

		InstanceData playerInstance;
		DirectX::XMStoreFloat4x4(&playerInstance.Transform, mTranspose);
		playerInstance.TextureIndex = m_player.textureID;
		playerInstance.Padding[0] = 0;
		playerInstance.Padding[1] = 0;
		playerInstance.Padding[2] = 0;

		InstanceDatas.emplace_back(playerInstance);
	}

	{
		DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&m_crosshair.scale);
		DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

		DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&m_crosshair.pos);
		DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

		// S縮放 * R旋轉 * T平移 這裡沒有旋轉
		DirectX::XMMATRIX mTransform = mScale * mTranslation;

		//轉置 C++ row major HLSL column major
		DirectX::XMMATRIX mTranspose = DirectX::XMMatrixTranspose(mTransform);

		InstanceData crosshairInstance;
		DirectX::XMStoreFloat4x4(&crosshairInstance.Transform, mTranspose);
		crosshairInstance.TextureIndex = m_crosshair.textureID;
		crosshairInstance.Padding[0] = 0;
		crosshairInstance.Padding[1] = 0;
		crosshairInstance.Padding[2] = 0;

		InstanceDatas.emplace_back(crosshairInstance);
	}
	//等等調整
	objectCounts += 2;					//player + crosshair
	objectCounts += m_enemys.size();	//enemy
	objectCounts += m_bullets.size();	//bullet

	instanceCounts = objectCounts;

	return objectCounts;
}


