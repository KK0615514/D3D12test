#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

// 3D物件頂點座標
struct MeshVertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 uv;

	// 適用於法線貼圖
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT4 tangent;
};

// 物件頂點資料 & 順序
struct MeshData {
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
};

// IFFT用 海洋頂點座標
struct OceanVertex {
	DirectX::XMFLOAT2 positionXZ;
};

// IFFT用 海洋頂點資料 & 順序
struct OceanMeshData {
	std::vector<OceanVertex> vertices;
	std::vector<uint32_t> indices;
};