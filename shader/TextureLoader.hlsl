struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD; 
};
struct VS_OUTPUT
{
    float4 pos : SV_POSITION; 
    float2 uv : TEXCOORD; 
    uint texIndex : TEX_INDEX; 
};

struct ObjectData
{
    matrix WorldMatrix;
    uint TextureIndex;
    uint3 padding;
};

Texture2D g_texture[] : register(t0, space0);
SamplerState g_sampler : register(s0); 
StructuredBuffer<ObjectData> gInstanceBuffer : register(t0, space1);

cbuffer PerFrameConstant : register(b0)
{
    matrix g_ViewProj; //***
    float3 cameraPos;
    float1 padding;
};

VS_OUTPUT VSMain(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    VS_OUTPUT output;

    ObjectData data = gInstanceBuffer[instanceID];

    float4 worldPos = mul(float4(input.pos, 1.0f), data.WorldMatrix);
    output.pos = mul(worldPos, g_ViewProj);

    output.uv = float2(input.uv.x, input.uv.y);
    output.texIndex = data.TextureIndex;

    return output;
}
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float4 color = g_texture[NonUniformResourceIndex(input.texIndex)].Sample(g_sampler, input.uv);
    return color;
}