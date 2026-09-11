#pragma once
#include <DirectXMath.h>

struct PerFrameConstant {
	DirectX::XMFLOAT4X4 View;				// 64 bytes camera
	DirectX::XMFLOAT4X4 Proj;				// 64 bytes camera
	DirectX::XMFLOAT4X4 ViewProj;			// 64 bytes camera
	DirectX::XMFLOAT3   CameraPos;			// 12 bytes camera
	float padding;							//  4 bytes
};

//複製貼上
struct TextRootConstants {
	uint32_t asciiCode;        // 文字的 ASCII 碼 (例如 'A' = 65)
	uint32_t textColor;        // 文字顏色 (如 0xFFFFFFFF)
	uint32_t fontTextureIndex;   // 文字圖集在描述符表中的索引 (填入 6)
	uint32_t padding;          // 填充對齊
};

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