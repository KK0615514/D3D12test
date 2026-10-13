#include "Game3DScene.h"

#include <DirectXMath.h>
#include <cstddef>
#include <stdexcept>

#include "system/CommonUtils.h"
#include "system/OceanMeshGenerator.h"

Game3DScene::Game3DScene(GameContext& ctx) :IScene(ctx)
{
    // 初始化時建立 IFFT用 海洋細分Mesh
    m_oceanMeshData = Ocean::GenerateOceanMesh(500.0f, 500.0f, 320, 320);   //100 64 目前沒chunk才用那麼大  

    m_camera = m_registry.create();
    m_registry.emplace<Transform>(m_camera);
    m_registry.emplace<Camera>(m_camera);
    auto& cameraTransform = m_registry.get<Transform>(m_camera);
    cameraTransform.position = { 0.0f, 20.0f, -30.0f };
    cameraTransform.SetRotationEuler(30.0f, 0.0f, 0.0f);

    //m_registry.emplace<Transform>(m_player);
}

void Game3DScene::Update() {

    m_renderData.oceanWaveParameters.Time += m_ctx.m_timer.GetDeltaTime();

    RotateCamera();
    MoveSystem();
    UpdateCamera();

    SpwanAndMoveEnemy();
    BuildOceanRenderData();
}



// test  打包相機 留下
void Game3DScene::UpdateCamera() {
    const float width = Common::InitialWindowWidth;
    const float height = Common::InitialWindowHeight;

    const auto& transform = m_registry.get<Transform>(m_camera);
    const auto& camera = m_registry.get<Camera>(m_camera);

    auto position = DirectX::XMLoadFloat3(&transform.position);
    auto rotation = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&transform.rotation));

    //  +Z 是前 +Y 是上
    auto forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), rotation);
    auto up = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 1, 0, 0), rotation);

    auto view = DirectX::XMMatrixLookToLH(position, forward, up);
    auto projection = DirectX::XMMatrixPerspectiveFovLH(
        camera.fovY,
        width / height,
        camera.nearZ,
        camera.farZ
    );

    auto& output = m_renderData.cameraData;
    DirectX::XMStoreFloat4x4(&output.ViewProj, DirectX::XMMatrixTranspose(view * projection));
    output.CameraPos = transform.position;
    output.padding = 0.0f;
}

// test  相機移動
void Game3DScene::MoveSystem() {

    auto delta = m_ctx.m_timer.GetDeltaTime();
    const float speed = 40.0f;
    auto& transform = m_registry.get<Transform>(m_camera);
    
    const auto rotation = DirectX::XMQuaternionNormalize(
        DirectX::XMLoadFloat4(&transform.rotation)
    );

    const auto forward = DirectX::XMVector3Rotate(
        DirectX::XMVectorSet(0, 0, 1, 0), rotation
    );
    const auto right = DirectX::XMVector3Rotate(
        DirectX::XMVectorSet(1, 0, 0, 0), rotation
    );

    float forwardInput = 0.0f;
    float rightInput = 0.0f;

    if (m_ctx.m_input.IsKeyHeld('W') ||
        m_ctx.m_input.IsKeyHeld(VK_LBUTTON))
        forwardInput += 1.0f;

    if (m_ctx.m_input.IsKeyHeld('S'))
        forwardInput -= 1.0f;

    if (m_ctx.m_input.IsKeyHeld('D'))
        rightInput += 1.0f;

    if (m_ctx.m_input.IsKeyHeld('A'))
        rightInput -= 1.0f;

    auto direction = DirectX::XMVectorAdd(
        DirectX::XMVectorScale(forward, forwardInput),
        DirectX::XMVectorScale(right, rightInput)
    );

    if (forwardInput != 0.0f || rightInput != 0.0f) {
        // 正規化 避免 W+D 斜向移動比較快
        direction = DirectX::XMVector3Normalize(direction);

        auto position = DirectX::XMLoadFloat3(&transform.position);
        position = DirectX::XMVectorAdd(
            position,
            DirectX::XMVectorScale(direction, speed * delta)
        );

        DirectX::XMStoreFloat3(&transform.position, position);
    }
}

// test  相機旋轉
void Game3DScene::RotateCamera() {
    const int mouseX = m_ctx.m_input.GetMouseX();
    const int mouseY = m_ctx.m_input.GetMouseY();

    if (!m_ctx.m_input.IsKeyHeld(VK_RBUTTON)) {
        m_cameraDragging = false;
        return;
    }

    // 剛開始拖曳，只記錄位置，避免視角跳動
    if (!m_cameraDragging) {
        m_lastMouseX = mouseX;
        m_lastMouseY = mouseY;  
        m_cameraDragging = true;
        return;
    }

    const float deltaX = static_cast<float>(mouseX - m_lastMouseX);
    const float deltaY = static_cast<float>(mouseY - m_lastMouseY);

    m_lastMouseX = mouseX;
    m_lastMouseY = mouseY;

    // 每像素轉動 0.1 度，不需要乘 deltaTime
    const float sensitivity = DirectX::XMConvertToRadians(0.22f);

    m_cameraYaw += deltaX * sensitivity;
    m_cameraPitch += deltaY * sensitivity;

    const float pitchLimit = DirectX::XMConvertToRadians(89.0f);
    if (m_cameraPitch > pitchLimit)
        m_cameraPitch = pitchLimit;
    if (m_cameraPitch < -pitchLimit)
        m_cameraPitch = -pitchLimit;

    // 避免 yaw 長時間累積成過大的數值
    if (m_cameraYaw > DirectX::XM_PI)
        m_cameraYaw -= DirectX::XM_2PI;
    if (m_cameraYaw < -DirectX::XM_PI)
        m_cameraYaw += DirectX::XM_2PI;

    auto& transform = m_registry.get<Transform>(m_camera);
    const auto rotation = DirectX::XMQuaternionRotationRollPitchYaw(
        m_cameraPitch,
        m_cameraYaw,
        0.0f
    );

    DirectX::XMStoreFloat4(&transform.rotation, rotation);
}

// test  加減生怪
void Game3DScene::SpwanAndMoveEnemy() {
    m_renderData.instanceDatas.clear();
    m_renderData.batches.clear();
    auto delta = m_ctx.m_timer.GetDeltaTime();
    //亂寫生怪
    for (int i = 1;i <= 1;i++)
    {
        float rx = Xoshiro128::Random() - 0.5f;
        float ry = Xoshiro128::Random() - 0.5f;
        float rz = Xoshiro128::Random() - 0.5f;
        entt::entity monkey = m_registry.create();
        m_registry.emplace<Transform>(monkey, Transform{
            .position = {rx * 200.0f,ry * 200.0f,rz * 200.0f}
            });
        m_registry.emplace<Speed>(monkey,
            Xoshiro128::RandomRange(-5.0f, 5.0f),
            Xoshiro128::RandomRange(-5.0f, 5.0f),
            Xoshiro128::RandomRange(-5.0f, 5.0f)
        );
        m_registry.emplace<Lifetime>(monkey, 10.0f);
    }

    //亂寫移動
    auto moveview = m_registry.view<Transform, Speed>();
    for (auto&& [entity, transform, speed] : moveview.each()) {
        transform = moveview.get<Transform>(entity);
        speed = moveview.get<Speed>(entity);
        transform.position.x += speed.x * delta;
        transform.position.y += speed.y * delta;
        transform.position.z += speed.z * delta;
    }

    // 複製貼上 存活確認
    std::vector<entt::entity> expired;
    auto dlview = m_registry.view<Lifetime>();

    // 確認存活與否 死人丟expired
    for (auto&& [entity, lifetime] : dlview.each()) {
        lifetime.time -= delta;
        if (lifetime.time <= 0.0f) {
            expired.push_back(entity);
        }
    }

    // 處刑
    for (const auto entity : expired) {
        m_registry.destroy(entity);
    }

    //資料寫進 m_renderData.instancedata 
    auto view = m_registry.view<Transform, Speed>();
    for (auto&& [entity, transform, speed] : view.each()) {

        // --- 以下完全繼承您原本的 DirectX 運算邏輯 ---
        DirectX::XMVECTOR vScale = DirectX::XMLoadFloat3(&transform.scale);
        DirectX::XMMATRIX mScale = DirectX::XMMatrixScalingFromVector(vScale);

        DirectX::XMVECTOR vPosition = DirectX::XMLoadFloat3(&transform.position); // 修正原本的 .pos 為 .position
        DirectX::XMMATRIX mTranslation = DirectX::XMMatrixTranslationFromVector(vPosition);

        // S縮放 * R旋轉 * T平移 (這裡引入您 Transform 結構體中的 rotation 矩陣)
        DirectX::XMVECTOR vOriginal = DirectX::XMLoadFloat4(&transform.rotation);

        // 2. 建立一個代表繞 X 軸旋轉 45 度的四元數
        DirectX::XMVECTOR vDelta = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(0.5f, 0.5f, 0.0f, 0.0f), 
            DirectX::XMConvertToRadians(3.0f)
        );

        // 3. 四元數相乘（這等於將旋轉疊加）
        DirectX::XMVECTOR vNewRotation = DirectX::XMQuaternionMultiply(vOriginal, vDelta);

        // 4. 將新結果存回結構體中（去掉錯誤的 & 符號）
        //DirectX::XMStoreFloat4(&transform.rotation, vNewRotation);

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

        m_renderData.instanceDatas.emplace_back(enemyInstance);
    }
    //moveSystem();

    const auto meshEnemy = m_meshes.find("test");

    if (meshEnemy != m_meshes.end())
    {
        m_renderData.batches.push_back(RenderBatch{
            .pso = PsoType::MeshDebug,
            .mesh = meshEnemy->second,
            .instanceOffset = 0,
            .instanceCount = static_cast<uint32_t>(m_renderData.instanceDatas.size()) 
        });
    }
}

// test  嘗試生成海 AI
void Game3DScene::BuildOceanRenderData() {
    const auto ocean = m_meshes.find("Ocean");
    if (ocean == m_meshes.end()) {
        return;
    }

    const uint32_t instanceOffset =
        static_cast<uint32_t>(m_renderData.instanceDatas.size());

    InstanceData instance{};

    // 單位矩陣：不縮放、不旋轉，保持原本 100 × 100 的尺寸。
    DirectX::XMStoreFloat4x4(
        &instance.Transform,
        DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity())
    );

    m_renderData.instanceDatas.push_back(instance);

    m_renderData.batches.push_back(RenderBatch{
        .pso = PsoType::Ocean,
        .mesh = ocean->second,
        .instanceOffset = instanceOffset,
        .instanceCount = 1
        });
}



void Game3DScene::SetOceanMesh(MeshHandle handle) {
    m_meshes.insert_or_assign("Ocean", handle);
}

const OceanMeshData& Game3DScene::GetOceanMeshData() const {
    return m_oceanMeshData;
}

void Game3DScene::SetMeshes(std::vector<MeshHandle> meshes) {
    // 檢查參數合不合規
    if (test.size() != meshes.size()) {
        throw std::invalid_argument("Game3DScene::SetMeshes : GPU回傳handle數量對不上 載入時的meshes數量");
    }

    for (size_t i = 0; i < test.size(); ++i) {
        m_meshes.insert_or_assign(test[i], meshes[i]);
    }
}



const SceneRenderData& Game3DScene::GetRenderData() const {
    return m_renderData;
}
