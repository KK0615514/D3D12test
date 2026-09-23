#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

// 2D 點座標
struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 uv;
};

// 3D物件頂點座標
struct MeshVertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;

	DirectX::XMFLOAT4 tangent;
};

// 物件頂點資料 & 順序
struct MeshData {
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
};

