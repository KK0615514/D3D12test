#pragma once
#include <string>
#include <vector>
#include <wrl/client.h>
#include <xaudio2.h>

// 音效資料結構
struct SoundEffect {
    WAVEFORMATEX wfx;
    std::vector<BYTE> audioData;
};

class SimpleAudio {
public:
    // 取得單例實例
    static SimpleAudio& Get() {
        static SimpleAudio instance;
        return instance;
    }

    // 初始化音訊引擎（在 D3D12 遊戲初始化時呼叫一次）
    bool Initialize();

    // 播放音效（支援多個音效同時重疊播放，不卡頓）
    void Play(const std::wstring& filePath);

    // 播放背景音樂（獨立軌道，循環播放，不會被普通音效中斷）
    void PlayBGM(const std::wstring& filePath);

    // 停止背景音樂
    void StopBGM();

    // 釋放所有資源（在遊戲關閉時呼叫）
    void Shutdown();

private:
    SimpleAudio() = default; // 隱藏建構子
    ~SimpleAudio() { Shutdown(); }

    bool LoadWaveFile(const std::wstring& filename, SoundEffect& outData);

    Microsoft::WRL::ComPtr<IXAudio2> m_xaudio2;
    IXAudio2MasteringVoice* m_masterVoice = nullptr;
    IXAudio2SourceVoice* m_bgmVoice = nullptr;

    // 用來記錄當前正在播放的普通音效軌道，方便自動清理
    std::vector<IXAudio2SourceVoice*> m_activeVoices;
};
