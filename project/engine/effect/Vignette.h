#pragma once
#include "effect/IPostEffect.h"

#include "math/Vector3.h"

#include <wrl/client.h>
#include <d3d12.h>

class Vignette : public IPostEffect {
public:
    Vignette() { name = "Vignette"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

    void SetColor(const Vector3& color) { paramData_->color = color; }
    void SetIntensity(float v) { paramData_->intensity = v; }
    void SetRadius(float v) { paramData_->radius = v; }
    void SetSoftness(float v) { paramData_->softness = v; }
    void SetPower(float v) { paramData_->power = v; }
    void SetAspect(float v) { paramData_->aspect = v; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    struct VignetteParam {
        Vector3 color;
        float intensity;
        float radius;
        float softness;
        float power;
        float aspect;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    VignetteParam* paramData_ = nullptr;
};