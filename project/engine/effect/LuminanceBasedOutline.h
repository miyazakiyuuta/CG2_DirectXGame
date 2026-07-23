// effect/LuminanceBasedOutline.h
#pragma once
#include "effect/IPostEffect.h"

#include "math/Vector3.h"

#include <wrl/client.h>
#include <d3d12.h>

// 輝度ベースのアウトライン（資料CG5 00_06）。
// シーン色の輝度に対するPrewittフィルタでエッジを検出する。
// 深度もprojectionInverseも不要なので、構成はVignetteと同じ t0+b0 のみ
class LuminanceBasedOutline : public IPostEffect {
public:
    LuminanceBasedOutline() { name = "LuminanceBasedOutline"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

    void SetLineColor(const Vector3& color) { paramData_->lineColor = color; }
    void SetStrength(float v) { paramData_->strength = v; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    struct OutlineParam { // HLSL側と一致（32byte）
        Vector3 lineColor; // 12
        float   strength;  //  4
        int32_t debugView; //  4
        float   pad0, pad1, pad2; // 12
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    OutlineParam* paramData_ = nullptr;
};
