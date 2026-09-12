// ==========================================
// 1. 資源與緩衝區宣告 (嚴格對齊 C++ 的 8 個 DWORD 結構)
// ==========================================

// Slot 0: 您的無界紋理陣列
Texture2D FontTextures[] : register(t0);
SamplerState TextureSampler : register(s1);

// Slot 2: 保留您原本 3D 物件的 PerFrameConstant（避免 Root Signature 報錯）
cbuffer PerFrameConstant : register(b0, space0)
{
    // ... 裡面原本的 3D 矩陣等變數請保留，文字渲染直接無視它即可 ...
};

// Slot 3: 文字/分數專用 Root Constants (同步 8 個 DWORD 結構)
cbuffer TextRootConstants : register(b1, space0)
{
    float screenX; // C++ 傳入：該數字在螢幕上的 X 像素座標
    float screenY; // C++ 傳入：該數字在螢幕上的 Y 像素座標
    float charWidth; // C++ 傳入：該數字顯示的寬度
    float charHeight; // C++ 傳入：該數字顯示的高度
    uint asciiCode; // C++ 傳入：該數字的 ASCII 碼 (例如 '6' = 54)
    uint textColor; // C++ 傳入：顏色 (例如 0xFFFFFFFF)
    uint fontTextureIndex; // C++ 傳入：文字貼圖索引 (6)
    uint padding;
};

// ==========================================
// 2. 接收外部 VB 的輸入佈局 (必須對齊您的 Input Layout)
// ==========================================
struct VS_INPUT
{
    float3 position : POSITION; // 您外面 -0.5 ~ 0.5 的四邊形頂點
    float2 localUV : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// ==========================================
// 3. Vertex Shader (完全動態變換)
// ==========================================
PS_INPUT VSMain(VS_INPUT input, uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    // A. 修正文字方向（將 -0.5~0.5 映射到 0~1 的乾淨局部坐標）
    float localX = input.position.x + 0.5f;
    float localY = 0.5f - input.position.y; // 翻轉 Y 軸修正倒立問題

    // B. 【核心改寫】：徹底移除硬編碼，改用 cbuffer 傳進來的動態變數計算像素座標
    float2 pixelPos;
    pixelPos.x = screenX + (localX * charWidth);
    pixelPos.y = screenY + (localY * charHeight);

    // C. 像素座標轉換為 D3D12 標準 NDC 座標 (-1 到 1)
    float ndcX = (pixelPos.x / 1280.0f) * 2.0f - 1.0f;
    float ndcY = 1.0f - (pixelPos.y / 720.0f) * 2.0f;

    output.position = float4(ndcX, ndcY, 0.0f, 1.0f);

    // D. 10x10 文字圖集動態裁剪 (以空格 ASCII 32 作為起點)
    int fontIndex = (int) asciiCode - 32;
    if (fontIndex < 0 || fontIndex >= 100)
    {
        fontIndex = 0;
    }

    uint col = fontIndex % 10;
    uint row = fontIndex / 10;

    float2 uvOffset = float2(col * 0.1f, row * 0.1f);
    
    // 套用修正好方向的局部坐標來做完美的 10x10 區塊 UV 映射
    output.uv = uvOffset + (float2(localX, localY) * 0.1f);

    return output;
}
// ==========================================
// 4. Pixel Shader (完全動態文字像素著色器)
// ==========================================
// 💡 請確保您的 CMake 或編譯指令將此處進入點設定為 "PS_Main"
// 💡 編譯參數範例：/E PS_Main /T ps_5_1

float4 PSMain(PS_INPUT input) : SV_Target
{
    // 1. 動態抓取描述符表（Descriptor Table）中由 C++ 傳進來的 fontTextureIndex 紋理（Index 6）
    // NonUniformResourceIndex 是 D3D12 硬體動態索引的安全修飾符
    float4 textureColor = FontTextures[NonUniformResourceIndex(fontTextureIndex)].Sample(TextureSampler, input.uv);
    
    // 2. 將 C++ 透過 Root Constants 傳進來的 32 位元整數顏色 (textColor) 
    // 解包成 RGBA 4 個浮點數 (0.0 ~ 1.0)，格式為 RGBA8
    float4 color;
    color.r = ((textColor >> 0) & 0xFF) / 255.0f;
    color.g = ((textColor >> 8) & 0xFF) / 255.0f;
    color.b = ((textColor >> 16) & 0xFF) / 255.0f;
    color.a = ((textColor >> 24) & 0xFF) / 255.0f;

    //textureColor.a = textureColor.r;

    // 3. 將去背後的紋理顏色與 C++ 傳入的目標顏色相乘
    // 配合您在 C++ 已經開啟的 BlendState (SrcAlpha / InvSrcAlpha)
    // 最終黑框會變全透明，且文字會染上您指定的顏色 (例如 0xFFFFFFFF 就是純白字)
    return textureColor * color;
}
