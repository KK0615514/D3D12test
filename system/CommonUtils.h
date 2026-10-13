#pragma once
#include <Windows.h>	// NOMINMAX , WIN32_LEAN_AND_MEAN 移動到cmake去宣告 可以減少Window.h部分不常用的 API
#include <stdexcept>	// runtime_error

#include <cstdint>
#include <vector>			//LoadBinaryFile
#include <fstream>			//LoadBinaryFile
#include <string>			//LoadBinaryFile

namespace Common {

	inline float InitialWindowWidth		= 1280.0f;

	inline float InitialWindowHeight	= 720.0f;

	//雙緩衝
	inline constexpr uint32_t BackBufferCount = 2;

	inline void ThrowIfFailed(HRESULT hr, const char text[]){
		if (FAILED(hr))
		{
			throw std::runtime_error(text);
		}
	}

    inline std::vector<char> LoadBinaryFile(const std::wstring& filename){
        std::ifstream file(
            filename,
            std::ios::binary | std::ios::ate
        );

        if (!file.is_open()) {
            throw std::runtime_error(
                "Common::LoadBinaryFile : 檔案開啟失敗"
            );
        }

        const std::streamsize size = file.tellg();

        if (size < 0) {
            throw std::runtime_error(
                "Common::LoadBinaryFile : 檔案大小取得失敗"
            );
        }
        if (size == 0) {
            throw std::runtime_error(
                "Common::LoadBinaryFile : 檔案為空"
            );
        }

        std::vector<char> buffer(static_cast<size_t>(size));

        if (!file.seekg(0, std::ios::beg)) {
            throw std::runtime_error(
                "Common::LoadBinaryFile : 檔案讀取位置設定失敗"
            );
        }
        if (!file.read(buffer.data(), size)) {
            throw std::runtime_error(
                "Common::LoadBinaryFile : 檔案讀取失敗"
            );
        }

        return buffer;
    }
}