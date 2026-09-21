#include "Game3DScene.h"

Game3DScene::Game3DScene() {
	entt::entity player = m_Registry.create();

}

void Game3DScene::Update() {

}







//拆掉IScene後移除
void Game3DScene::Updateaaa() {

}
std::unique_ptr<IScene> Game3DScene::ChangeScene() {
	return 0;
}
uint32_t Game3DScene::PackInstanceData() {
	return 0;
}