#pragma once
#include <DirectXMath.h>
//#include "CollisionTypes.h"

struct Enemy {
public:
	DirectX::XMFLOAT3 pos{};
	DirectX::XMFLOAT3 scale{ 75.0f, 75.0f, 1.0f };
	uint32_t textureID;

	int HP;

	DirectX::XMFLOAT2 dir{};
	float speed{};
	float angle{};
private:

};