#include "Game3DScene.h"

Game3DScene::Game3DScene() {
    m_camera = m_registry.create();
    m_registry.emplace<Transform>(m_camera);
    m_registry.emplace<Camera>(m_camera);

	entt::entity monkey = m_registry.create();
    m_registry.emplace<Transform>(monkey);

	instanceCounts = 1;
}

void Game3DScene::Update() {
    InstanceDatas.clear();

	InstanceData GpuData{};
    auto view = m_registry.view<Transform>();
    if (!view.empty()) {
        auto entity = view.front(); // 這就會抓到當初那隻猴子的實體 ID
        auto& transform = view.get<Transform>(entity);

        // --- 以下完全繼承您原本的 DirectX 運算邏輯 ---
        DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&transform.scale);
        DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

        DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&transform.position); // 修正原本的 .pos 為 .position
        DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

        // S縮放 * R旋轉 * T平移 (這裡引入您 Transform 結構體中的 rotation 矩陣)
        DirectX::XMVECTOR vOriginal = DirectX::XMLoadFloat4(&transform.rotation);

        // 2. 建立一個代表繞 X 軸旋轉 45 度的四元數
        DirectX::XMVECTOR vDelta = DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), DirectX::XMConvertToRadians(1.0f));

        // 3. 四元數相乘（這等於將旋轉疊加）
        DirectX::XMVECTOR vNewRotation = DirectX::XMQuaternionMultiply(vOriginal, vDelta);

        // 4. 將新結果存回結構體中（去掉錯誤的 & 符號）
        DirectX::XMStoreFloat4(&transform.rotation, vNewRotation);

        // 5. 轉換為矩陣供後續繪圖使用
        DirectX::XMMATRIX mRotation = DirectX::XMMatrixRotationQuaternion(vNewRotation);

        // 矩陣相乘：縮放 -> 旋轉 -> 平移
        DirectX::XMMATRIX mTransform = mScale * mRotation * mTranslation;

        // 轉置 C++ row major HLSL column major
        DirectX::XMMATRIX mTranspose = DirectX::XMMatrixTranspose(mTransform);

        InstanceData enemyInstance;
        DirectX::XMStoreFloat4x4(&enemyInstance.Transform, mTranspose);

        // 帶入貼圖 ID
        enemyInstance.TextureIndex = 0;

        enemyInstance.Padding[0] = 0;
        enemyInstance.Padding[1] = 0;
        enemyInstance.Padding[2] = 0;

        InstanceDatas.emplace_back(enemyInstance);
    }

}

