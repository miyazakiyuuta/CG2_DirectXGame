#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "DirectXCommon.h"
#include <unordered_map>
#include <DirectXPackedVector.h>

class SrvManager;

// テクスチャマネージャー
class TextureManager {
public:
	// シングルトンインスタンスの取得
	static TextureManager* GetInstance();
	// 終了
	void Finalize();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>画像イメージデータ</returns>
	void LoadTexture(const std::string& filePath);

	// メタデータの取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	// SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);
	// GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

private:
	// テクスチャ1枚分のデータ
	struct TextureData {
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas_;

	SrvManager* srvManager_ = nullptr;
};

