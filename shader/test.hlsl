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

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
};

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance = gInstances[instanceID];

    VSOutput output;
    float4 worldPosition = mul(float4(input.position, 1.0f),
                               instance.Transform);
    output.position = mul(worldPosition, ViewProj);
    output.color = abs(input.normal);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}