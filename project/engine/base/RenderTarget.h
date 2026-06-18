#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

class SrvManager;

class RenderTarget {
public:

    /// テクスチャ作成
    void Create(ID3D12Device* device, SrvManager* srvManager, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, 
        uint32_t width, uint32_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    /// 描画開始（バリア遷移 + RTセット + クリア）
    void BeginRender(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);

    /// 描画終了（バリア遷移）
    void EndRender(ID3D12GraphicsCommandList* cmdList);

    /// ImGui::Image() に渡す用
    uint32_t GetSrvIndex() const { return srvIndex_; }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:

    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ = {};
    uint32_t srvIndex_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    //float clearColor_[4] = { 0.1f, 0.25f, 0.5f, 1.0f };
    float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};