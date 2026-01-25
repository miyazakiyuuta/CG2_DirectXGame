#pragma once
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <dxcapi.h>
#include <array>
#include <string>
#include <chrono>

class WinApp;

// DirectX基盤
class DirectXCommon {
public: // メンバ関数
	// 初期化
	void Initialize(WinApp* winApp);

	// 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

	// シェーダーのコンパイル
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// バッファリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// <summary>
	/// テクスチャリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/// <summary>
	/// テクスチャリソースの転送
	/// </summary>
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UpLoadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// デスクリプタヒープを生成する
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	// 転送用などで、今詰んでいるコマンドを即実行して完了まで待つ
	void ExecuteCommandListAndWait();

	// 最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	/* getter */
	ID3D12Device* GetDevice() { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() { return commandList_.Get(); }
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	size_t GetSwapChainResourceNum()const { return swapChainResources_.size(); }

private:
	// デバイスの初期化
	void InitializeDevice();
	// コマンド関連の初期化
	void InitializeCommand();
	// スワップチェーンの生成
	void CreateSwapChain();
	// 深層バッファの生成
	void CreateDepthBuffer();
	// 各種デスクリプタヒープの生成
	void CreateDescriptorHeaps();
	// レンダーターゲットビューの初期化
	void InitializeRenderTargetView();
	/// 深度ステンシルビューの初期化
	void InitializeDepthStencilView();
	// フェンスの初期化
	void InitializeFence();
	// ビューポート矩形の初期化
	void InitializeViewportRect();
	// シザリング矩形の初期化 
	void InitializeScissorRect();
	// DXCコンパイラの生成
	void CreateDXCCompiler();
	// ImGuiの初期化
	void InitializeImGui();

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource();

	/// <summary>
	/// 指定番号のCPUデスクリプタハンドルを取得する
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// 指定番号のGPUデスクリプタハンドルを取得する
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	// FPS固定初期化
	void InitializeFixFPS();
	// FPS固定更新
	void UpdateFixFPS();
	

private:
	HRESULT hr_;
	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	// DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	// スワップチェーンを生成する
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	// WindowsAPI
	WinApp* winApp_ = nullptr;
	// デスクリプタサイズ
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;
	// デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	const uint32_t kBackBufferCount = 2;
	// SwapChainからResourceを引っ張ってくる
	//Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];
	// スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
	// 深度ステンシルバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
	uint64_t fenceValue_;
	HANDLE fenceEvent_;
	// ビューポート矩形
	D3D12_VIEWPORT viewport_{};
	// シザリング矩形
	D3D12_RECT scissorRect_;
	// DXCユーティリティ
	IDxcUtils* dxcUtils_ = nullptr;
	// DXCコンパイラ
	IDxcCompiler3* dxcCompiler_ = nullptr;
	// デフォルトインクルードハンドラ
	IDxcIncludeHandler* includeHandler_ = nullptr;
	//static IDxcIncludeHandler* includeHandler_;

	// TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier_{};

	// 記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

	
};

