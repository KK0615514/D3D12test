struct InstanceData
{
    float4x4 Transform;
    uint TextureIndex;
    uint3 Padding;
};

StructuredBuffer<InstanceData> gInstances : register(t0, space1);

cbuffer PerFrameConstant : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPos;
    float Padding;
};

cbuffer PerDrawConstant : register(b1)
{
    uint InstanceOffset;
};

// 單位皆為世界距離，目前約定 1 單位 = 1 公尺。
static const float OceanSeaLevel = 0.0f;
static const float2 OceanSimulationOriginXZ = float2(-250.0f, -250.0f);
static const float2 OceanSimulationSizeXZ = float2(500.0f, 500.0f);

Texture2D<float> gOceanHeight : register(t0, space2);
SamplerState gOceanHeightSampler : register(s0, space2);

float SampleOceanHeight(float2 oceanUV)
{
    uint width;
    uint height;
    gOceanHeight.GetDimensions(width, height);

    // 約定 texel (i, j) 表示模擬座標 (i / width, j / height)。
    // 加半個 texel，讓該座標取樣到 texel 中心。
    float2 texelSize = 1.0f / float2(width, height);

    return gOceanHeight.SampleLevel(
        gOceanHeightSampler,
        oceanUV + 0.5f * texelSize,
        0.0f
    );
}

float2 WorldXZToOceanUV(float2 worldXZ)
{
    return (worldXZ - OceanSimulationOriginXZ)/ OceanSimulationSizeXZ;
}

float3 CalculateOceanNormal(float2 oceanUV)
{
    uint width;
    uint height;
    gOceanHeight.GetDimensions(width, height);

    float2 texelSize = 1.0f / float2(width, height);

    float heightLeft = SampleOceanHeight(oceanUV - float2(texelSize.x, 0.0f));

    float heightRight = SampleOceanHeight(oceanUV + float2(texelSize.x, 0.0f));

    float heightBack = SampleOceanHeight(oceanUV - float2(0.0f, texelSize.y));

    float heightFront = SampleOceanHeight(oceanUV + float2(0.0f, texelSize.y));

    // 相鄰高度取樣點之間的世界距離。
    float2 sampleSpacing = OceanSimulationSizeXZ * texelSize;

    // 左右、前後各跨兩個取樣間距。
    float slopeX = (heightRight - heightLeft) / (2.0f * sampleSpacing.x);

    float slopeZ = (heightFront - heightBack) / (2.0f * sampleSpacing.y);

    // 平坦時坡度為零，法線朝 +Y。
    return normalize(float3(-slopeX, 1.0f, -slopeZ));
}

struct VSInput
{
    float2 positionXZ : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 oceanUV : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
};

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance = gInstances[InstanceOffset + instanceID];

    float3 localPosition = float3(
        input.positionXZ.x,
        0.0f,
        input.positionXZ.y
    );

    float4 worldPosition = mul(
        float4(localPosition, 1.0f),
        instance.Transform
    );

    float2 oceanUV = WorldXZToOceanUV(worldPosition.xz);

    float waveHeight = SampleOceanHeight(oceanUV);
    worldPosition.y = OceanSeaLevel + waveHeight;

    VSOutput output;
    output.position = mul(worldPosition, ViewProj);
    output.oceanUV = oceanUV;
    output.worldNormal = CalculateOceanNormal(oceanUV);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // 插值後的法線需要重新正規化。
    float3 normal = normalize(input.worldNormal);

    // 從海面指向光源的方向。
    float3 toLight = normalize(float3(-1.0f, 0.25f, -0.6f));

    float diffuse = saturate(dot(normal, toLight));

    // 少量環境光，讓背光的坡面不至於全黑。
    float lighting = 0.05f + 1.5f * diffuse;

    static const bool ShowUV = true;

    float2 uv = frac(input.oceanUV);
    float3 baseColor = ShowUV
        ? float3(uv.x, uv.y, 0.2f)
        : float3(0.0f, 0.55f, 0.65f);

    return float4(baseColor * lighting, 1.0f);
}