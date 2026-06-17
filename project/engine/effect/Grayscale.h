#pragma once
#include "effect/IPostEffect.h"
#include <wrl/client.h>
#include <d3d12.h>

class Grayscale : public IPostEffect {
public:
    Grayscale() { name = "Grayscale"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};