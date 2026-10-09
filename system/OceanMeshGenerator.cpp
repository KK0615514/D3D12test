#include "system/OceanMeshGenerator.h"

OceanMeshData Ocean::GenerateOceanMesh(float width, float depth, uint32_t cellsX, uint32_t cellsZ) {
    // 檢查參數是否合規
    if (!std::isfinite(width) || !std::isfinite(depth) ||width <= 0.0f || depth <= 0.0f){
        throw std::invalid_argument("GenerateOceanMesh: 非法的 width 與 depth 值");
    }
    if (cellsX == 0 || cellsZ == 0){
        throw std::invalid_argument("GenerateOceanMesh: 細分數量要大於0");
    }

    const uint64_t columns = uint64_t(cellsX) + 1;
    const uint64_t rows = uint64_t(cellsZ) + 1;

    // 先檢查乘法前的上限 避免計數溢位 限制為 D3D12 buffer view 可表示的大小
    const uint64_t maxVertices = UINT32_MAX / sizeof(OceanVertex);
    const uint64_t maxIndices = UINT32_MAX / sizeof(uint32_t);
    if (columns > maxVertices / rows    ||     uint64_t(cellsX) > (maxIndices / 6) / cellsZ){
        throw std::overflow_error("GenerateOceanMesh: 超出buffer的最大上限");
    }

    const uint64_t vertexCount = columns * rows;
    const uint64_t indexCount = uint64_t(cellsX) * cellsZ * 6;

    OceanMeshData mesh;
    mesh.vertices.reserve(static_cast<size_t>(vertexCount));
    mesh.indices.reserve(static_cast<size_t>(indexCount));

    // 排列方式 先X 後Z
    for (uint32_t z = 0; z <= cellsZ; ++z)
    {
        const float v = static_cast<float>(z) / cellsZ;
        const float positionZ = (v - 0.5f) * depth;

        for (uint32_t x = 0; x <= cellsX; ++x)
        {
            const float u = static_cast<float>(x) / cellsX;
            const float positionX = (u - 0.5f) * width;

            mesh.vertices.push_back(
                OceanVertex{ { positionX, positionZ } });
        }
    }

    const uint32_t stride = static_cast<uint32_t>(columns);

    for (uint32_t z = 0; z < cellsZ; ++z)
    {
        for (uint32_t x = 0; x < cellsX; ++x)
        {
            // a ─ b
            // │   │
            // c ─ d
            const uint32_t a = z * stride + x;
            const uint32_t b = a + 1;
            const uint32_t c = a + stride;
            const uint32_t d = c + 1;

            // 在 XZ 平面上，叉積方向朝 +Y。
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);

            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    return mesh;
}