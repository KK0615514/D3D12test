#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "TextureManager.h"

void TextureManager::Init() {
	m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//建立SRV Heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
	srvHeapDesc.NumDescriptors = 256;												//儲存256張圖片 256個descriptor
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));
}

TextureHandle TextureManager::LoadTexture(const char* path, ID3D12GraphicsCommandList1* m_commandList)
{
	int w, h, channel;
	unsigned char* img = stbi_load(path , &w, &h, &channel, 4);

	//撰寫適用於圖片的Resource desc
	D3D12_RESOURCE_DESC textureDesc{};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = w;
	textureDesc.Height = h;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//設定GPU與CPU的權限 此為預設 僅供GPU存取
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	ComPtr<ID3D12Resource> texture;
	m_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture)
	);

	//建立上傳用的圖片buffer 手動版本計算part 
	uint32_t bpp = 4;		//bytes per pixel,RGBA8 = 4
	uint32_t unalignedRowPitch = w * bpp;

	uint32_t alignedRowPitch = (unalignedRowPitch + 255) & ~255;	//& ~255來捨棄小於255的值 確保為256的倍數 硬體限制
	uint64_t uploadBufferSize = static_cast<uint64_t>(alignedRowPitch * h);		//計算總需求

	//撰寫適用於上傳用的Buffer desc
	D3D12_RESOURCE_DESC uploadDesc{};
	uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadDesc.Width = uploadBufferSize;
	uploadDesc.Height = 1;					//buffer是一維
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels = 1;
	uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;		upload用只能為預設值 即左邊這個

	//設定GPU與CPU的權限 CPU可寫入 GPU可讀取
	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	//建立上傳圖片用的Buffer 空值
	ComPtr<ID3D12Resource> uploadBuffer;
	m_device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)
	);

	//指定可以寫入Upload Buffer記憶體的指標
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0,0 };
	uploadBuffer->Map(0, &readRange, &mappedData);
	//dest = destination
	unsigned char* pDest = reinterpret_cast<unsigned char*>(mappedData);	//底層愛用reinterpret_cast
	for (uint32_t i = 0;i < h;++i)
	{
		// 計算 CPU img 端這一行的起始點（使用未對齊的 Pitch）
		unsigned char* srcRow = img + (i * unalignedRowPitch);
		// 計算 GPU Upload Buffer 端這一行的起始點（使用對齊後的 Pitch）
		unsigned char* destRow = pDest + (i * alignedRowPitch);
		// 只複製「真正的圖片像素資料」，後面未滿 256 的部分放著不管（硬體會自動忽略）
		memcpy(destRow, srcRow, unalignedRowPitch);
	}
	uploadBuffer->Unmap(0, nullptr);

	//設定來源端布局
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint{};
	bufferFootprint.Offset = 0; // 從 0 開始讀
	bufferFootprint.Footprint.Width = w;
	bufferFootprint.Footprint.Height = h;
	bufferFootprint.Footprint.Depth = 1;
	bufferFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 與建立 Texture 時相同的格式
	bufferFootprint.Footprint.RowPitch = alignedRowPitch; // 使用剛剛手動對齊的 Pitch！

	//設定來源端 upload buffer
	D3D12_TEXTURE_COPY_LOCATION srcLocation{};
	srcLocation.pResource = uploadBuffer.Get();								// 來源是 uploadBuffer
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;		// 類型指定為帶有 Footprint 的 Buffer
	srcLocation.PlacedFootprint = bufferFootprint;

	//設定目的端 Texture Resource
	D3D12_TEXTURE_COPY_LOCATION destLocation{};
	destLocation.pResource = texture.Get();									// 目的地是 Texture
	destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;		// 類型是 Subresource 索引
	destLocation.SubresourceIndex = 0;									// 複製到 Mip Level 0 

	//執行複製
	m_commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);

	//複製進Texture Manager 的temp裡面 確保Upload buffer 不會銷毀
	m_tempUploadBuffers.push_back(uploadBuffer);

	//切換texture resource狀態 從copy_dest -> shader resource
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // 轉為 Shader 可讀
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//還沒用fence確認完成沒 不能release
	m_commandList->ResourceBarrier(1, &barrier);

	// 建立SRV (綁定buffer與handle)
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	srvHandle.ptr += (size_t)m_allocatedCount * m_descriptorSize; // 指針偏移到空欄

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);

	//回傳資訊到 Texture Pool
	TextureData texData;
	texData.resource = texture;
	texData.descriptorIndex = m_allocatedCount;
	m_texturePool.push_back(texData);

	//Count調整 
	uint32_t currentIndex = m_allocatedCount;
	m_allocatedCount++;
	
	//釋放圖片資源
	stbi_image_free(img);

	return TextureHandle{ currentIndex };
}

void TextureManager::ClearUploadBuffer() {
	m_tempUploadBuffers.clear();
}
