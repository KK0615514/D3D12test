#pragma once
#include <DirectXMath.h>

struct alignas(16) InstanceData {
	DirectX::XMFLOAT4X4 Transform;				// 64 bytes
	uint32_t			TextureIndex;			//  4 bytes
	uint32_t			Padding[3];				// 12 bytes
};