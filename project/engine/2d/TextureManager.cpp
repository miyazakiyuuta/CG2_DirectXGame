#include "2d/TextureManager.h"
#include "base/SrvManager.h"
#include "utility/StringUtility.h"

using namespace StringUtility;
using namespace Microsoft::WRL;

TextureManager* TextureManager::instance = nullptr;

TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
	CreateDefaultTexture();
}

void TextureManager::LoadTexture(const std::string& filePath) {
	if (filePath.empty() || textureDatas_.contains(filePath))
		return;

	std::wstring filePathW = ConvertString(filePath);
	DirectX::ScratchImage image{};
	HRESULT hr;

	if (filePathW.ends_with(L".dds")) {
		hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else {
		hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}

	TextureData& textureData = textureDatas_[filePath];
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	// 【修正】ローカル変数ではなく構造体メンバへ保存。転送完了までリソースを維持させる。
	textureData.intermediateResource = dxCommon_->UpLoadTextureData(textureData.resource, mipImages);

	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);
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
	uint32_t white = 0xFFFFFFFF;
	DirectX::ScratchImage image;
	image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
	std::memcpy(image.GetImages()->pixels, &white, sizeof(uint32_t));

	TextureData& textureData = textureDatas_[kDefaultTextureName];
	textureData.metadata = image.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	textureData.intermediateResource = dxCommon_->UpLoadTextureData(textureData.resource, image);
	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);
}

void TextureManager::Finalize() {
	delete instance;
	instance = nullptr;
}