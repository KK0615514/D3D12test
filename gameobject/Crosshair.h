#pragma once
#include <DirectXMath.h>

struct Crosshair {
	DirectX::XMFLOAT3 pos{};
	DirectX::XMFLOAT3 scale{ 75.0f, 75.0f, 1.0f };
	uint32_t textureID;

	float moveX;
	float moveY;
};