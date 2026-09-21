#pragma once
#include <memory>
#include <vector>

#include "../system/Timer.h"
#include "../system/Random.h"
#include "../system/InputManager.h"

//可刪除
#include "../system/ShaderStructureBuffer.h" 
//可刪除
#include "../gameobject/Enemy.h"
#include "../gameobject/Player.h"
#include "../gameobject/Bullet.h"
#include "../gameobject/Crosshair.h"
#include "../gameobject/UI.h"
#include "../gameobject/Text.h"

inline int finalscore{0};

enum class SceneType {
    Menu,
    Game,
    Result
};

class IScene {
public:
    std::vector<InstanceData> InstanceDatas;
    uint32_t instanceCounts;

    Player m_player{};
    Crosshair m_crosshair{};
    std::vector<Enemy> m_enemys;
    std::vector<Bullet>m_bullets;
    std::vector <UI> m_UI;
    int m_score{0};
    SceneType currentScene;

    virtual ~IScene() = default;
    virtual void Updateaaa() = 0;
    virtual std::unique_ptr<IScene> ChangeScene() = 0;
    virtual uint32_t PackInstanceData() = 0;
};