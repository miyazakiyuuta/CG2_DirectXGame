#include "2d/TextureManager.h"
#include "utility/StringUtility.h"
#include "base/SrvManager.h"

using namespace StringUtility;
using namespace Microsoft::WRL;

TextureManager* TextureManager::instance = nullptr;

TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Finalize() {
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	// SRVの数と同数
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);

	CreateDefaultTexture();
}

void TextureManager::LoadTexture(const std::string& filePath) {
	
	if (filePath.empty()) { // 空パスなら何もしない
		return;
	}

	// 読み込み済みテクスチャを検索
	if (textureDatas_.contains(filePath)) {
		return;
	}

	std::wstring filePathW = ConvertString(filePath);
	DirectX::ScratchImage image{};
	HRESULT hr;

	// 拡張子による読み込み関数の分岐
	if (filePathW.ends_with(L".dds")) { // .ddsで終わっていたらdssとみなす。(より安全な方法はある)
		hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else { // それ以外(WIC)として読み込む
		hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	}
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};

	if (DirectX::IsCompressed(image.GetMetadata().format)) { // 圧縮フォーマットかどうか調べる
		mipImages = std::move(image); // 圧縮フォーマットならそのまま使う
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}

	TextureData& textureData = textureDatas_[filePath];
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	// テクスチャ枚数上限チェック
	assert(srvManager_->CanAllocate());
	// SRV確保
	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UpLoadTextureData(textureData.resource, mipImages);

	dxCommon_->ExecuteCommandListAndWait();

	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);
	//srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
	auto it = textureDatas_.find(filePath);
	assert(it != textureDatas_.end());
	return it->second.metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
	auto it = textureDatas_.find(filePath);
	assert(it != textureDatas_.end());
	return it->second.srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {
	auto it = textureDatas_.find(filePath);
	assert(it != textureDatas_.end());
	return it->second.srvHandleGPU;
}

void TextureManager::CreateDefaultTexture() {
	uint32_t white = 0xFFFFFFFF; // 1x1ピクセルの白データ(RGBA8)

	DirectX::ScratchImage image;
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
	assert(SUCCEEDED(hr));

	// 画素データ書き込み
	std::memcpy(image.GetImages()->pixels, &white, sizeof(uint32_t));

	// TextureDataを構築してmapに登録
	TextureData& textureData = textureDatas_[kDefaultTextureName];
	textureData.metadata = image.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	
	// SRV確保と転送
	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UpLoadTextureData(textureData.resource, image);

	dxCommon_->ExecuteCommandListAndWait();

	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);

}
