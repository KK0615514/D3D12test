#include "Audio.h"
#include <fstream>
#include <algorithm>

#pragma comment(lib, "xaudio2.lib")

bool SimpleAudio::Initialize() {
    // 1. 初始化 COM 元件
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != S_FALSE) return false;

    // 2. 建立 XAudio2 引擎
    hr = XAudio2Create(&m_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return false;

    // 3. 建立主混音聲音（代表實體喇叭輸出）
    hr = m_xaudio2->CreateMasteringVoice(&m_masterVoice);
    if (FAILED(hr)) return false;

    return true;
}

void SimpleAudio::Play(const std::wstring& filePath) {
    if (!m_xaudio2) return;

    // 清理已經播放完畢的舊軌道（防止記憶體無限暴增）
    m_activeVoices.erase(
        std::remove_if(m_activeVoices.begin(), m_activeVoices.end(), [](IXAudio2SourceVoice* voice) {
            XAUDIO2_VOICE_STATE state;
            voice->GetState(&state);
            if (state.BuffersQueued == 0) {
                voice->DestroyVoice(); // 播放完了，銷毀軌道
                return true;
            }
            return false;
            }),
        m_activeVoices.end()
    );

    // 載入 WAV 檔案
    SoundEffect* sound = new SoundEffect(); // 這裡為了簡化動態配置，實務上建議做快取
    if (!LoadWaveFile(filePath, *sound)) {
        delete sound;
        return;
    }

    // 建立一個全新的獨立音源軌道（這就是多聲道的關鍵！）
    IXAudio2SourceVoice* sourceVoice = nullptr;
    HRESULT hr = m_xaudio2->CreateSourceVoice(&sourceVoice, &sound->wfx);
    if (FAILED(hr)) {
        delete sound;
        return;
    }

    // 綁定音訊資料
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = static_cast<UINT32>(sound->audioData.size());
    buffer.pAudioData = sound->audioData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.pContext = sound; // 把記憶體指標掛在 context，方便日後釋放

    // 提交並馬播放
    sourceVoice->SubmitSourceBuffer(&buffer);
    sourceVoice->Start(0);

    // 丟進管理陣列
    m_activeVoices.push_back(sourceVoice);
}

void SimpleAudio::PlayBGM(const std::wstring& filePath) {
    if (!m_xaudio2) return;
    StopBGM(); // 如果原本有 BGM 正在播，先停掉
    
    SoundEffect* sound = new SoundEffect();
    if (!LoadWaveFile(filePath, *sound)) {
        delete sound;
        // 🎯 加上這行提示，找不到檔案時會跳出警告視窗
        MessageBoxW(NULL, L"音效檔案載入失敗，請檢查路徑！", L"錯誤", MB_OK);
        return;
    }

    HRESULT hr = m_xaudio2->CreateSourceVoice(&m_bgmVoice, &sound->wfx);
    if (FAILED(hr)) {
        delete sound;
        return;
    }

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = static_cast<UINT32>(sound->audioData.size());
    buffer.pAudioData = sound->audioData.data();
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE; // 🎯 背景音樂設定無限循環
    buffer.pContext = sound;

    m_bgmVoice->SubmitSourceBuffer(&buffer);
    m_bgmVoice->Start(0);
}

void SimpleAudio::StopBGM() {
    if (m_bgmVoice) {
        // 🎯 安全修正：先徹底停止硬體播放
        m_bgmVoice->Stop(0);
        m_bgmVoice->FlushSourceBuffers();

        XAUDIO2_VOICE_STATE state;
        m_bgmVoice->GetState(&state);
        if (state.pCurrentBufferContext) {
            // 釋放記憶體
            delete static_cast<SoundEffect*>(state.pCurrentBufferContext);
        }
        m_bgmVoice->DestroyVoice();
        m_bgmVoice = nullptr;
    }
}

void SimpleAudio::Shutdown() {
    StopBGM();
    for (auto voice : m_activeVoices) {
        XAUDIO2_VOICE_STATE state;
        voice->GetState(&state);
        if (state.pCurrentBufferContext) {
            delete static_cast<SoundEffect*>(state.pCurrentBufferContext);
        }
        voice->DestroyVoice();
    }
    m_activeVoices.clear();

    if (m_masterVoice) {
        m_masterVoice->DestroyVoice();
        m_masterVoice = nullptr;
    }
    m_xaudio2.Reset();
    CoUninitialize();
}

// 輔助函式：讀取基礎 WAV 檔案
bool SimpleAudio::LoadWaveFile(const std::wstring& filename, SoundEffect& outData) {
    HMMIO hmmio = mmioOpenW(const_cast<LPWSTR>(filename.c_str()), nullptr, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio) {
        std::wstring msg = L"【檔案開啟失敗】請檢查路徑是否存在：\n" + filename;
        MessageBoxW(NULL, msg.c_str(), L"SimpleAudio 錯誤", MB_OK | MB_ICONERROR);
        return false;
    }

    MMCKINFO riffChunk = { 0 };
    riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &riffChunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    MMCKINFO subChunk = { 0 };
    subChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &subChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    // 🎯 關鍵修正 1：先將結構體完全清零，避免殘留隨機亂碼記憶體
    ZeroMemory(&outData.wfx, sizeof(WAVEFORMATEX));

    // 🎯 關鍵修正 2：FFmpeg 轉出來的 PCM 格式大小可能大於標準的 16 或 18 位元組
    // 我們只讀取標準 WAVEFORMATEX 的大小，避免覆蓋掉後續未知的記憶體區塊
    LONG sizeToRead = (subChunk.cksize > sizeof(WAVEFORMATEX)) ? sizeof(WAVEFORMATEX) : subChunk.cksize;

    if (mmioRead(hmmio, reinterpret_cast<HPSTR>(&outData.wfx), sizeToRead) == -1) {
        mmioClose(hmmio, 0);
        return false;
    }

    // 如果檔案中的 fmt 區塊比較大，把剩下沒讀完的格式資料跳過
    if (subChunk.cksize > sizeToRead) {
        mmioSeek(hmmio, subChunk.cksize - sizeToRead, SEEK_CUR);
    }

    mmioAscend(hmmio, &subChunk, 0);

    subChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(hmmio, &subChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    // 🎯 關鍵修正 3：確保安全配置記憶體大小
    outData.audioData.resize(subChunk.cksize);
    if (mmioRead(hmmio, reinterpret_cast<HPSTR>(outData.audioData.data()), subChunk.cksize) == -1) {
        mmioClose(hmmio, 0);
        return false;
    }

    mmioClose(hmmio, 0);
    return true;
}
