#pragma once
#include <DirectXMath.h>

struct UI {
	DirectX::XMFLOAT3 pos{};
	DirectX::XMFLOAT3 scale{ 600.0f, 300.0f, 1.0f };
	uint32_t textureID;
};