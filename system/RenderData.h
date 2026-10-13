#pragma once
#include <cstdint>
#include <vector>

#include "ecs_component/ShaderStructureBuffer.h"    // InstanceData
#include "ecs_component/ShaderConstantBuffer.h"     // PerFrameConstant

#include "system/PsoType.h"
#include "system/RenderHandle.h"

// 物件渲染用的pso, mesh, instansData的offset以及數量
struct RenderBatch {
    PsoType pso;
    MeshHandle mesh;
    uint32_t instanceOffset = 0;
    uint32_t instanceCount = 0;
};

struct SceneRenderData {
    OceanWaveParameters oceanWaveParameters{};      //test
    PerFrameConstant cameraData{};
    std::vector<InstanceData> instanceDatas;
    std::vector<RenderBatch> batches;
};

/*
RenderBatch未來可能增加 texture handle
    可直接加 現在先不
    多材質貼圖可參考 material texture 現在先不考慮
*/