#pragma once
#include <DirectXMath.h>
//#include "CollisionTypes.h"

struct Bullet {
public:
	DirectX::XMFLOAT3 pos{};
	DirectX::XMFLOAT3 scale{ 40.0f, 40.0f, 1.0f };
	uint32_t textureID;

	int atk;

	DirectX::XMFLOAT2 dir{};
	float speed{};
	float angle{};
private:

};