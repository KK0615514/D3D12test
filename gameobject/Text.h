#pragma once
#include "cstdint"

struct Text {
    float screenX;             // 文字在畫面上的 X 像素座標
    float screenY;             // 文字在畫面上的 Y 像素座標
    float charWidth;           // 單個字體寬度（像素）
    float charHeight;          // 單個字體高度（像素）
    uint32_t asciiCode;        // 文字的 ASCII 碼（例如 '6' = 54）
    uint32_t textColor;        // 文字顏色（如 0xFFFFFFFF）
    uint32_t fontTextureIndex; // 文字圖集在描述符表中的索引（填入 6）
    uint32_t padding;          // 填充對齊
};