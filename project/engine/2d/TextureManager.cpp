#include "2d/TextureManager.h"
#include "base/SrvManager.h"
#include "utility/StringUtility.h"
#include "utility/Logger.h"
#include <filesystem>

using namespace StringUtility;
using namespace Microsoft::WRL;

namespace fs = std::filesystem;

TextureManager* TextureManager::instance = nullptr;

TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Finalize() {}

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

	// Log attempt
	Logger::Log(std::string("TextureManager::LoadTexture: ") + filePath + "\n");

	// 読み込み済みテクスチャを検索
	if (textureDatas_.contains(filePath)) {
		return;
	}

	// Check file exists early to give clearer diagnostics
	if (filePath != TextureManager::kDefaultTextureName) {
		fs::path p(filePath);
		if (!fs::exists(p)) {
			Logger::Log(std::string("Texture file not found: ") + filePath + "\n");
			return; // fallback: do not assert, leave default handling to caller
		}
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
	if (FAILED(hr)) {
		Logger::Log(std::string("Texture load failed (LoadFrom...): ") + filePath + " hr=" + std::to_string(static_cast<long long>(hr)) + "\n");
		return;
	}

	DirectX::ScratchImage mipImages{};

	if (DirectX::IsCompressed(image.GetMetadata().format)) { // 圧縮フォーマットかどうか調べる
		mipImages = std::move(image);                        // 圧縮フォーマットならそのまま使う
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
		if (FAILED(hr)) {
			Logger::Log(std::string("GenerateMipMaps failed for: ") + filePath + " hr=" + std::to_string(static_cast<long long>(hr)) + " - falling back to single-level image\n");
			// Fallback: use original image as single mip level so loading can continue
			try {
				mipImages = std::move(image);
			} catch (...) {
				Logger::Log(std::string("Fallback assignment failed for: ") + filePath + "\n");
				return;
			}
		}
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

	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);
}

void TextureManager::ReloadTexture(const std::string& filePath) {
	auto it = textureDatas_.find(filePath);
	if (it == textureDatas_.end()) {
		LoadTexture(filePath);   // 未ロードなら通常ロード
		return;
	}
	TextureData& textureData = it->second;
	const uint32_t srvIndex = textureData.srvIndex;

	// 画像を読み直す
	std::wstring filePathW = ConvertString(filePath);
	DirectX::ScratchImage image{};
	HRESULT hr = filePathW.ends_with(L".dds")
		? DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)
		: DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
			image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}

	// 古いリソースは GPU 完了後に破棄（in-flight 参照の use-after-free 防止）
	ComPtr<ID3D12Resource> oldResource = textureData.resource;

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

	ComPtr<ID3D12Resource> intermediate = dxCommon_->UpLoadTextureData(textureData.resource, mipImages);

	srvManager_->CreateSRVForTexture(srvIndex, textureData.resource.Get(), textureData.metadata);
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

	srvManager_->CreateSRVForTexture(textureData.srvIndex, textureData.resource.Get(), textureData.metadata);
}
