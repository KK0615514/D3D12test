#pragma once
#include <DirectXMath.h>
//#include "CollisionTypes.h"

struct Player {
public:
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 scale;
	uint32_t textureID;

	float moveX{0};
	float moveY{0};

	int HP;
	int atk;

	bool isInvincibleCD = false;
	float invincibleTimer = 0.0f;
	float invincibleCD;

private:

};