#pragma once
#include "effect/IPostEffect.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <cstddef>

// 畳み込み系ポストエフェクトの共通基底。

class ConvolutionEffect : public IPostEffect {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;

protected:
    // 派生クラスが実装するフック
    virtual const wchar_t* GetPixelShaderPath() const = 0;
    virtual void InitializeParam() = 0; // CreateParamBufferを呼び、初期値を書き込む

    // 派生クラスが自分のCBufferを作るためのヘルパー。
    // 呼ぶと paramResource_ を生成し、mappedParam_ にマップ先が入る。
    void CreateParamBuffer(size_t size);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    void* mappedParam_ = nullptr; // 派生クラスで自分のParam型にreinterpret_castして使う

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
};
