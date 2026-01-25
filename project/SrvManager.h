#pragma once
#include "DirectXCommon.h"

// SRV管理
class SrvManager {
public:
	void Initialize(DirectXCommon* dxCommon);

	bool CanAllocate() const;
	uint32_t Allocate();

	ID3D12DescriptorHeap* GetDescriptorHeap()const { return descriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// SRV生成(テクスチャ用)
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	// SRV作成(Structured Buffer用)
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

private:
	DirectXCommon* dxCommon_ = nullptr;

	// 最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;
	// SRV用のデスクリプタサイズ
	uint32_t descriptorSize_ = 0;
	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;
	

};

