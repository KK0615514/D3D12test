#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>	// 前面兩個ifndef 再瘦身windows
#include <stdexcept>	// runtime_error

#include <vector>			//LoadBinaryFile
#include <fstream>			//LoadBinaryFile
#include <string>			//LoadBinaryFile

namespace Common {
	//雙緩衝
	inline constexpr uint32_t BackBufferCount = 2;

	inline void ThrowIfFailed(HRESULT hr, const char text[])
	{
		if (FAILED(hr))
		{
			throw std::runtime_error(text);
		}
	}

	inline std::vector<char> LoadBinaryFile(const std::wstring& filename) {
		// 以「二進位(ios::binary)」且「直接移到檔案末尾(ios::ate)」的方式開啟檔案
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		if (!file.is_open())
		{
			// 檔案開啟失敗（可能是路徑錯了，或是忘記把 .cso 複製到執行檔路徑）
			// 實務上可以在這裡跳出錯誤訊息
			return {};
		}

		// 因為開啟時直接移到了檔案末尾，tellg() 剛好就是整份檔案的總大小（位元組）
		std::streamsize size = file.tellg();

		// 根據檔案大小，開闢對應容量的陣列空間
		std::vector<char> buffer(size);

		// 將檔案讀取指針移回最開頭，準備讀取數據
		file.seekg(0, std::ios::beg);

		// 一口氣把整份檔案的二進位數據讀入我們的 buffer 陣列中
		if (file.read(buffer.data(), size))
		{
			return buffer;
		}

		return {};
	}

}