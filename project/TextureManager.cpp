#include "TextureManager.h"
#include "StringUtility.h"

using namespace StringUtility;
using namespace Microsoft::WRL;

TextureManager* TextureManager::instance = nullptr;
// ImGuiで0番を使用するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

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

void TextureManager::Initialize(DirectXCommon* dxCommon) {
	dxCommon_ = dxCommon;

	// SRVの数と同数
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath) {
	/*
	// 読み込み済みテクスチャを検索
	auto it = std::find_if(
		textureDatas_.begin(),
		textureDatas_.end(),
		[&](TextureData& textureData) {return textureData.filePath == filePath; }
	);

	if (it != textureDatas_.end()) {
		// 読み込み済みなら早期return
		return;
	}
	*/

	// テクスチャ枚数上限チェック
	assert(textureDatas_.size() + kSRVIndexTop < DirectXCommon::kMaxSRVCount);

	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// テクスチャデータを追加
	textureDatas_.resize(textureDatas_.size() + 1);
	// 追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureDatas_.back();
	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

	// テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas_.size() - 1) + kSRVIndexTop;

	textureData.srvHandleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
	textureData.srvHandleGPU = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

	ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UpLoadTextureData(textureData.resource, mipImages);
	
	//ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	// commandListをCloseし、commandListQueue->ExecuteCommandListsを使いキックする
	// 実行を待つ
	// 実行が完了したので、allocatorとcommandListをResetして次のコマンドを積めるようにする
	// ここまできたら転送は終わっているので、intermediateResourceはReleaseしても良い

	// ここで今積んでいるコピーコマンドを実行して完了待ち
	dxCommon_->ExecuteCommandListAndWait();
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) {
	// 読み込み済みテクスチャを検索
	auto it = std::find_if(
		textureDatas_.begin(),
		textureDatas_.end(),
		[&](TextureData& textureData) {return textureData.filePath == filePath; }
	);

	if (it != textureDatas_.end()) {
		// 読み込み済みなら要素番号を返す
		uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas_.begin(), it));
		return textureIndex;
	}
	assert(0);
	return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex) {
	// テクスチャ番号が正常範囲内であるかチェック
	assert(textureIndex < textureDatas_.size());

	// テクスチャデータの参照を取得
	TextureData& textureData = textureDatas_[textureIndex];

	// そのテクスチャのGPUハンドルを返す
	return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex) {
	// 範囲外指定違反チェック
	// テクスチャ番号が正常範囲内である
	assert(textureIndex < textureDatas_.size());

	// テクスチャデータの参照を取得
	TextureData& textureData = textureDatas_[textureIndex];
	return textureData.metadata;
}
