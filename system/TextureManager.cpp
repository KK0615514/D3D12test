#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "TextureManager.h"

#include <memory>
#include <cstring>
#include <cassert>
#include <utility>
#include <limits>
#include <stdexcept>

#include "system/CommonUtils.h"

ComPtr<ID3D12Resource> TextureManager::CreateDefaultTexture(
	const D3D12_RESOURCE_DESC& desc,D3D12_RESOURCE_STATES initialState,HeapAllocation& allocation) {
	if (!m_device || !m_heapManager) {
		throw std::logic_error("TextureManager::CreateDefaultTexture : 缺少 Device 或 HeapManager");
	}
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width == 0 || desc.Height == 0) {
		throw std::invalid_argument("TextureManager::CreateDefaultTexture : 需要有效的 Texture2D 描述");
	}

	const HeapAllocation allocated = m_heapManager->Allocate(
		desc,
		D3D12_HEAP_TYPE_DEFAULT
	);

	ComPtr<ID3D12Resource> resource;

	Common::ThrowIfFailed(
		m_device->CreatePlacedResource(
			m_heapManager->GetHeap(allocated.heapId),
			allocated.offset,
			&desc,
			initialState,
			nullptr,
			IID_PPV_ARGS(&resource)
		),
		"TextureManager::CreateDefaultTexture : Placed texture 建立失敗"
	);

	allocation = allocated;
	return resource;
}

UploadSlice TextureManager::StageUploadData(
	const D3D12_RESOURCE_DESC& desc, const unsigned char* sourceData, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint) {
	// 此入口處理 STB 解碼後的單層、單 mip RGBA8 圖片。
	if (!sourceData ||
		desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		desc.Width == 0 || desc.Height == 0 ||
		desc.DepthOrArraySize != 1 ||
		desc.MipLevels != 1 ||
		desc.SampleDesc.Count != 1 ||
		desc.SampleDesc.Quality != 0 ||
		desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
		throw std::invalid_argument("TextureManager::StageUploadData : 無效來源資料或 RGBA8 Texture2D 描述");
	}
	if (!m_device || !m_uploadManager) {
		throw std::logic_error("TextureManager::StageUploadData : 缺少 Device 或 UploadManager");
	}
	if (desc.Width > std::numeric_limits<uint64_t>::max() / 4) {
		throw std::overflow_error("TextureManager::StageUploadData : 圖片行大小溢位");
	}

	UINT numRows = 0;
	UINT64 rowBytes = 0;
	UINT64 totalBytes = 0;

	// 重設輸出布局，避免保留前一次呼叫的欄位值。
	footprint = {};

	// BaseOffset 先用 0，取得單張圖片所需的布局與容量。
	m_device->GetCopyableFootprints(
		&desc,
		0,
		1,
		0,
		&footprint,
		&numRows,
		&rowBytes,
		&totalBytes
	);

	if (totalBytes == 0 ||
		totalBytes == UINT64_MAX ||
		numRows != desc.Height ||
		rowBytes != desc.Width * 4 ||
		rowBytes > footprint.Footprint.RowPitch) {
		throw std::invalid_argument("TextureManager::StageUploadData : 無效 texture copy 布局");
	}

	const UploadSlice slice = m_uploadManager->Allocate(
		totalBytes,
		D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
	);

	// STB 的來源資料是緊密排列的 RGBA8。
	// UPLOAD 端則使用 GetCopyableFootprints 回傳的 RowPitch。
	for (UINT row = 0; row < numRows; ++row) {
		const auto* sourceRow =
			sourceData +
			static_cast<size_t>(row) *
			static_cast<size_t>(rowBytes);

		auto* destinationRow =
			slice.cpuAddress +
			static_cast<size_t>(row) *
			footprint.Footprint.RowPitch;

		std::memcpy(
			destinationRow,
			sourceRow,
			static_cast<size_t>(rowBytes)
		);
	}

	// copy location 使用大 buffer 內的切片 offset。
	footprint.Offset = slice.offset;

	return slice;
}



TextureLoadResult TextureManager::LoadTexture(const char* name, ID3D12GraphicsCommandList1* copyList) {
	if (!name || name[0] == '\0' || !copyList) {
		throw std::invalid_argument("TextureManager::LoadTexture : 無效名稱或 CopyList");
	}
	if (!m_device || !m_uploadManager) {
		throw std::logic_error("TextureManager::LoadTexture : 缺少 Device 或 UploadManager");
	}
	if (copyList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
		throw std::invalid_argument("TextureManager::LoadTexture : 需要 Copy CommandList");
	}

	const std::string key = name;

	const auto cached = m_textureCache.find(key);
	if (cached != m_textureCache.end()) {
		return TextureLoadResult{
			cached->second,
			false
		};
	}

	if (m_textures.size() >= UINT32_MAX) {
		throw std::overflow_error("TextureManager::LoadTexture : Handle 數量超出上限");
	}

	const std::string path ="asset/texture/" + key + ".png";

	int width = 0;
	int height = 0;
	int channels = 0;

	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> image (
		stbi_load(path.c_str(), &width, &height, &channels, 4),
		&stbi_image_free
	);

	if (!image) {
		const char* reason = stbi_failure_reason();
		throw std::runtime_error(
			std::string("TextureManager::LoadTexture : 圖片載入失敗，") +
			(reason ? reason : "未知原因")
		);
	}
	if (width <= 0 || height <= 0) {
		throw std::runtime_error("TextureManager::LoadTexture : 無效圖片尺寸");
	}

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = static_cast<uint64_t>(width);
	desc.Height = static_cast<uint32_t>(height);
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// Copy queue 使用時會隱式提升至 COPY_DEST。
	// Copy 批次完成後回到 COMMON，供 Direct queue 接手。
	auto textureData = BuildTexture2D(
		desc,
		D3D12_RESOURCE_STATE_COMMON,
		false
	);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};

	const UploadSlice uploadSlice = StageUploadData(
		desc,
		image.get(),
		footprint
	);

	// 先保存切片與註冊 texture，成功後才錄製 copy。
	m_pendingUploadBuffers.push_back(uploadSlice);

	TextureHandle handle{ UINT32_MAX };

	try {
		handle = RegisterTexture(key,std::move(textureData));
	}
	catch (...) {
		m_pendingUploadBuffers.pop_back();
		throw;
	}

	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = uploadSlice.resource;
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	source.PlacedFootprint = footprint;

	D3D12_TEXTURE_COPY_LOCATION target{};
	target.pResource = m_textures[handle.id].resource.Get();
	target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	target.SubresourceIndex = 0;

	copyList->CopyTextureRegion(
		&target,
		0, 0, 0,
		&source,
		nullptr
	);

	return TextureLoadResult{
		handle,
		true
	};
}

TextureHandle TextureManager::CreateTexture2D(
	const std::string& name, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav){
	if (name.empty() || m_textureCache.contains(name)) {
		throw std::invalid_argument("TextureManager::CreateTexture2D : 名稱為空或已存在");
	}

	auto data = BuildTexture2D(desc, initialState, createUav);
	return RegisterTexture(name, std::move(data));
}

TextureGpuResource  TextureManager::BuildTexture2D(
	const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, bool createUav) {
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
	if (!m_device || !m_heapManager || !m_descriptors) {
		throw std::logic_error("TextureManager::BuildTexture2D : 缺少 Device、HeapManager 或 DescriptorManager");
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

	TextureGpuResource data{};
	data.state = initialState;

	data.resource = CreateDefaultTexture(
		desc,
		initialState,
		data.allocation
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

TextureHandle TextureManager::RegisterTexture(
	const std::string& name, TextureGpuResource&& data) {

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
		if (!m_textureCache.emplace(name, handle).second) {
			throw std::logic_error("TextureManager::RegisterTexture : 名稱已存在");
		}
	}
	catch (...) {
		m_textures.pop_back();
		throw;
	}

	return handle;
}

void TextureManager::ClearUploadBuffers() {
	m_pendingUploadBuffers.clear();
}



TextureUploadCheckpoint TextureManager::GetUploadCheckpoint() const{
	return TextureUploadCheckpoint{
		m_textures.size(),
		m_pendingUploadBuffers.size()
	};
}

void TextureManager::RollbackUpload(const TextureUploadCheckpoint& checkpoint)  {
	assert(
		checkpoint.textureCount <= m_textures.size() &&
		checkpoint.pendingCount <= m_pendingUploadBuffers.size() &&
		"TextureManager::RollbackUpload : 無效 checkpoint"
	);

	for (auto it = m_textureCache.begin();it != m_textureCache.end(); ) {
		if (it->second.id >= checkpoint.textureCount) {
			it = m_textureCache.erase(it);
		}
		else {
			++it;
		}
	}

	while (m_textures.size() > checkpoint.textureCount) {
		m_textures.pop_back();
	}

	while (m_pendingUploadBuffers.size() > checkpoint.pendingCount) {
		m_pendingUploadBuffers.pop_back();
	}
}



// UAV不是用Trasition
void TextureManager::UavBarrier(
	TextureHandle handle, ID3D12GraphicsCommandList1* directList){
	if (!directList ||directList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT){
		throw std::invalid_argument("TextureManager::UavBarrier : 需要有效的 Direct CommandList");
	}
	if (handle.id >= m_textures.size()) {
		throw std::out_of_range("TextureManager::UavBarrier : 無效 Handle");
	}

	const auto& texture = m_textures[handle.id];
	if (!texture.resource ||texture.state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
		throw std::logic_error("TextureManager::UavBarrier : Resource 不存在或不處於 UAV 狀態");
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = texture.resource.Get();

	directList->ResourceBarrier(1, &barrier);
}

// 切換Transition Barrier的外部入口
void TextureManager::TransitionTexture(
	TextureHandle handle, ID3D12GraphicsCommandList1* directList, D3D12_RESOURCE_STATES targetState) {
	if (handle.id >= m_textures.size()) {
		throw std::out_of_range("TextureManager::TransitionTexture : 無效 Handle");
	}

	RecordTransition(
		m_textures[handle.id],
		directList,
		targetState
	);
}

// 切換Transition Barrier的內部入口
void TextureManager::RecordTransition(
	TextureGpuResource& texture, ID3D12GraphicsCommandList1* directList, D3D12_RESOURCE_STATES targetState)
{
	if (!directList || directList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT){
		throw std::invalid_argument("TextureManager::RecordTransition : 需要有效的 Direct CommandList");
	}
	if (!texture.resource) {
		throw std::logic_error("TextureManager::RecordTransition : Resource 不存在");
	}
	if (texture.state == targetState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture.resource.Get();
	barrier.Transition.Subresource =D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = texture.state;
	barrier.Transition.StateAfter = targetState;

	directList->ResourceBarrier(1, &barrier);

	// 更新已錄製指令後預期的狀態，不表示 GPU 已執行完成。
	texture.state = targetState;
}



uint32_t TextureManager::GetSrvIndex(TextureHandle handle) const {
	const auto* data = TryGetTexture(handle);

	if (!data || data->srv.index == UINT32_MAX) {
		throw std::out_of_range("TextureManager::GetSrvIndex : 無效 Handle 或缺少 SRV");
	}

	return data->srv.index;
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