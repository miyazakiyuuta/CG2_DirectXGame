#pragma once
#include "DirectXTex.h"
#include "base/DirectXCommon.h"
#include <DirectXPackedVector.h>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class SrvManager;

// テクスチャマネージャー
class TextureManager {
public:
	static TextureManager* GetInstance();
	void Finalize();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	void LoadTexture(const std::string& filePath);

	// メタデータの取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	// SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);
	// GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	static inline const std::string kDefaultTextureName = "white";

private:
	// テクスチャ1枚分のデータ
	struct TextureData {
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		// 【重要】GPUへの転送が完了するまでデータを維持するためのリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	void CreateDefaultTexture();

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<std::string, TextureData> textureDatas_;
	SrvManager* srvManager_ = nullptr;
};