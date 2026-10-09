#pragma once
#include <cmath>
#include <stdexcept>

#include "system/MeshData.h"

namespace Ocean {
    OceanMeshData GenerateOceanMesh(float width, float depth, uint32_t cellsX, uint32_t cellsZ);
}

