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

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // 暫時把模型的 XY 當成畫面座標；0.5 是測試用縮放。
    output.position = float4(input.position.xy * 0.5f, 0.5f, 1.0f);
    output.color = abs(input.normal);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}