#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "TextureManager.h"

#include <memory>
#include <cstring>
#include <string>
#include <utility>
#include <stdexcept>

#include "system/CommonUtils.h"

TextureLoadResult TextureManager::LoadTexture(const char* name, ID3D12GraphicsCommandList1* commandList)
{
	// 檢查名稱
	if (!name || name[0] == '\0' || !commandList) {
		throw std::invalid_argument("TextureManager::LoadTexture : 無效名稱或 CommandList");
	}
	// 檢查是否是Direct Queue
	if (commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT) {
		throw std::invalid_argument("TextureManager::LoadTexture : 目前需使用 Direct CommandList");
	}

	const std::string key = name;
	const auto cachedHandle = m_textureCache.find(key);

	if (cachedHandle != m_textureCache.end()) {
		return TextureLoadResult{ cachedHandle->second, false};
	}

	const std::string path = "asset/texture/" + key + ".png";

	int w, h, channel;
	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> image(	// 這種寫法可以在離開scope之後呼叫stbi_image_free釋放image
		stbi_load(path.c_str() , &w, &h, &channel, 4),
		&stbi_image_free
	);
	unsigned char* img = image.get();
	if (img == nullptr) {
		const char* reason = stbi_failure_reason();
		throw std::runtime_error(
			std::string("TextureManager::LoadTexture : 圖片載入失敗，") +
			(reason ? reason : "未知原因")
		);
	}
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("TextureManager::LoadTexture : 無效圖片尺寸");
	}

	// RGBA8，每個像素 4 bytes。
	const uint64_t rowBytes = uint64_t(w) * 4;
	const uint64_t alignedRowBytes = (rowBytes + 255) & ~uint64_t(255);

	if (alignedRowBytes > UINT32_MAX) {
		throw std::overflow_error("TextureManager::LoadTexture : Row pitch 超出上限");
	}

	const uint32_t unalignedRowPitch = static_cast<uint32_t>(rowBytes);
	const uint32_t alignedRowPitch = static_cast<uint32_t>(alignedRowBytes);
	const uint64_t uploadBufferSize = uint64_t(alignedRowPitch) * uint64_t(h);

	D3D12_RESOURCE_DESC textureDesc{};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = static_cast<uint64_t>(w);
	textureDesc.Height = static_cast<uint32_t>(h);
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	auto texData = BuildTexture2D(textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, false);
	ID3D12Resource* texture = texData.resource.Get();

	D3D12_RESOURCE_DESC uploadDesc{};
	uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadDesc.Width = uploadBufferSize;
	uploadDesc.Height = 1;
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels = 1;
	uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//設定GPU與CPU的權限 CPU可寫入 GPU可讀取
	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	//建立上傳圖片用的Buffer 空值
	ComPtr<ID3D12Resource> uploadBuffer;
	Common::ThrowIfFailed(
		m_device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)
		),
		"TextureManager::LoadTexture : Upload buffer 建立失敗"
	);

	//指定可以寫入Upload Buffer記憶體的指標
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0,0 };
	Common::ThrowIfFailed(
		uploadBuffer->Map(0, &readRange, &mappedData),
		"TextureManager::LoadTexture : Upload buffer Map 失敗"
	);
	//dest = destination
	auto* destination = static_cast<unsigned char*>(mappedData);	//底層愛用reinterpret_cast
	for (uint32_t row = 0;row < static_cast<uint32_t>(h);++row)
	{
		// 計算 CPU img 端這一行的起始點（使用未對齊的 Pitch）
		const unsigned char* srcRow = img + size_t(row) * unalignedRowPitch;
		// 計算 GPU Upload Buffer 端這一行的起始點（使用對齊後的 Pitch）
		unsigned char* destRow = destination + size_t(row) * alignedRowPitch;
		// 只複製「真正的圖片像素資料」，後面未滿 256 的部分放著不管（硬體會自動忽略）
		memcpy(destRow, srcRow, unalignedRowPitch);
	}
	uploadBuffer->Unmap(0, nullptr);

	//設定來源端布局
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	footprint.Footprint.Width = static_cast<uint32_t>(w);
	footprint.Footprint.Height = static_cast<uint32_t>(h);
	footprint.Footprint.Depth = 1;
	footprint.Footprint.Format = textureDesc.Format; // 與建立 Texture 時相同的格式
	footprint.Footprint.RowPitch = alignedRowPitch; // 使用剛剛手動對齊的 Pitch！

	//設定來源端 upload buffer
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = uploadBuffer.Get();								// 來源是 uploadBuffer
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;		// 類型指定為帶有 Footprint 的 Buffer
	source.PlacedFootprint = footprint;

	//設定目的端 Texture Resource
	D3D12_TEXTURE_COPY_LOCATION target{};
	target.pResource = texture;									// 目的地是 Texture
	target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;		// 類型是 Subresource 索引
	target.SubresourceIndex = 0;									// 複製到 Mip Level 0 

	//複製進Texture Manager 的temp裡面 確保Upload buffer 不會銷毀
	m_pendingUploadBuffers.push_back(std::move(uploadBuffer));

	//執行複製
	commandList->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);

	//切換texture resource狀態 從copy_dest -> shader resource
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // 轉為 Shader 可讀
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//還沒用fence確認完成沒 不能release
	commandList->ResourceBarrier(1, &barrier);

	texData.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	const TextureHandle handle = RegisterTexture(key, std::move(texData));

	return TextureLoadResult{ handle, true };
}

TextureHandle TextureManager::CreateTexture2D(const std::string& name, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav){
	if (name.empty() || m_textureCache.contains(name)) {
		throw std::invalid_argument("TextureManager::CreateTexture2D : 名稱為空或已存在");
	}

	auto data = BuildTexture2D(desc, initialState, createUav);
	return RegisterTexture(name, std::move(data));
}

TextureGpuResource  TextureManager::BuildTexture2D(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav) {
	if (!m_device || !m_descriptors) {
		throw std::logic_error("TextureManager::BuildTexture2D : 缺少 Device 或 DescriptorManager");
	}
	// 初版只支援單層、單 mip、非 MSAA 的 Texture2D。
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		desc.Width == 0 || desc.Height == 0 ||
		desc.DepthOrArraySize != 1 ||
		desc.MipLevels != 1 ||
		desc.SampleDesc.Count != 1 ||
		desc.SampleDesc.Quality != 0 ||
		desc.Layout != D3D12_TEXTURE_LAYOUT_UNKNOWN) {
		throw std::invalid_argument("TextureManager::BuildTexture2D : 不支援的 Texture2D 規格");
	}
	if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format != DXGI_FORMAT_R32_FLOAT) {
		throw std::invalid_argument("TextureManager::BuildTexture2D : 不支援的格式");
	}
	if (desc.Flags & (D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
		throw std::invalid_argument("TextureManager::BuildTexture2D : 不支援的資源 Flags");
	}
	if (createUav && !(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
		throw std::invalid_argument("TextureManager::BuildTexture2D : UAV 需要 ALLOW_UNORDERED_ACCESS");
	}

	TextureGpuResource  data{};
	data.state = initialState;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	Common::ThrowIfFailed(
		m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			initialState,
			nullptr,
			IID_PPV_ARGS(&data.resource)
		),
		"TextureManager::BuildTexture2D : 貼圖建立失敗"
	);

	data.srv = m_descriptors->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(
		data.resource.Get(),
		&srvDesc,
		data.srv.cpu
	);

	if (createUav) {
		data.uav = m_descriptors->Allocate();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		m_device->CreateUnorderedAccessView(
			data.resource.Get(),
			nullptr,
			&uavDesc,
			data.uav.cpu
		);
		data.hasUav = true;
	}
	return data;
}

TextureHandle TextureManager::RegisterTexture(const std::string& name, TextureGpuResource&& data) {

	if (name.empty()) {
		throw std::invalid_argument("TextureManager::RegisterTexture : 名稱不可為空");
	}
	if (m_textureCache.contains(name)) {
		throw std::logic_error("TextureManager::RegisterTexture : 名稱已存在");
	}
	if (m_textures.size() >= UINT32_MAX) {
		throw std::overflow_error("TextureManager::RegisterTexture : Handle 數量超出上限");
	}

	const TextureHandle handle{
		static_cast<uint32_t>(m_textures.size())
	};

	m_textures.push_back(std::move(data));

	try {
		m_textureCache.emplace(name, handle);
	}
	catch (...) {
		m_textures.pop_back();
		throw;
	}

	return handle;
}

void TextureManager::ClearUploadBuffer() {
	m_pendingUploadBuffers.clear();
}



const TextureGpuResource* TextureManager::TryGetTexture(TextureHandle handle) const{
	if (handle.id >= m_textures.size()) {
		return nullptr;
	}

	const auto& data = m_textures[handle.id];

	if (!data.resource) {
		return nullptr;
	}

	return &data;
}

const TextureGpuResource* TextureManager::TryGetTexture(const std::string& name) const{
	const auto found = m_textureCache.find(name);

	if (found == m_textureCache.end()) {
		return nullptr;
	}

	return TryGetTexture(found->second);
}

uint32_t TextureManager::GetSrvIndex(TextureHandle handle) const {
	const auto* data = TryGetTexture(handle);

	if (!data || data->srv.index == UINT32_MAX) {
		throw std::out_of_range("TextureManager::GetSrvIndex : 無效 Handle 或缺少 SRV");
	}

	return data->srv.index;
}