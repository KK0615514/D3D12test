#pragma once
#include <DirectXMath.h>

struct PerFrameConstant {
	//DirectX::XMFLOAT4X4 View;				// 64 bytes camera
	//DirectX::XMFLOAT4X4 Proj;				// 64 bytes camera
	DirectX::XMFLOAT4X4 ViewProj;			// 64 bytes camera
	DirectX::XMFLOAT3   CameraPos;			// 12 bytes camera
	float padding;							//  4 bytes
};

// test
struct OceanWaveParameters {
	float Time = 0.0f;
	float Amplitude = 15.0f;
	float AngularSpeed = 1.0f;
	float Padding = 0.0f;
};
static_assert(sizeof(OceanWaveParameters) == 16);

struct PerViewConstant {

};

struct PerMaterialConstant {

};

// 萬用的對齊公式：把任何 size 對齊到 alignment 的倍數
//template<typename T>
//constexpr T AlignUp(T size, T alignment) {
//	return (size + alignment - 1) & ~(alignment - 1);
//}
//
// 實際使用方式：
//size_t bufferSize = AlignUp(sizeof(cameraConstant), 256);