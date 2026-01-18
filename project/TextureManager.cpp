#include "TextureManager.h"
#include "StringUtility.h"
#include "SrvManager.h"

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
	delete instance;
	instance = nullptr;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	// SRVの数と同数
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath) {
	
	// 読み込み済みテクスチャを検索
	if (textureDatas_.contains(filePath)) {
		return;
	}

	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

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

	// ここで今積んでいるコピーコマンドを実行して完了待ち
	dxCommon_->ExecuteCommandListAndWait();

	srvManager_->CreateSRVForTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
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
