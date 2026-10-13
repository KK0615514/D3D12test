RWTexture2D<float> gOceanHeight : register(u0, space2);

cbuffer OceanWaveParameters : register(b2)
{
    float Time;
    float Amplitude;
    float AngularSpeed;
    float Padding;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    gOceanHeight.GetDimensions(width, height);

    if (id.x >= width || id.y >= height){
        return;
    }

    float2 uv = float2(id.xy) / float2(width, height);

    static const float TwoPi = 6.28318530718f;

    // X 方向 8 個週期，Z 方向 6 個週期。
    // 整數週期確保高度圖重複取樣時，波形連續。
    float phaseX = TwoPi * 8.0f * uv.x - AngularSpeed * Time;
    float phaseZ = TwoPi * 6.0f * uv.y - AngularSpeed * 0.7f * Time;

    float waveHeight = Amplitude * (
        0.65f * sin(phaseX) +
        0.35f * sin(phaseZ)
    );

    gOceanHeight[id.xy] = waveHeight;
}