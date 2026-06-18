#pragma once
#include "effect/IPostEffect.h"

#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Matrix4x4.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>

class RadialBlur : public IPostEffect {
public:
    RadialBlur() { name = "RadialBlur"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Update(float deltaTime) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

    // --- 中心点 ---
    void SetCenterUV(const Vector2& uv) { paramData_->center = uv; }
    void SetCenterWorld(const Vector3& worldPos, const Matrix4x4& viewProj);

    // --- 基本パラメータ ---
    void SetIntensity(float v) { baseIntensity_ = v; } // 常時掛けのベース強度
    void SetBlurWidth(float v) { paramData_->blurWidth = v; }
    void SetNumSamples(int v) { paramData_->numSamples = v; }
    void SetFocusRadius(float v) { paramData_->focusRadius = v; }
    void SetFocusSoftness(float v) { paramData_->focusSoftness = v; }
    void SetAspect(float v) { paramData_->aspect = v; }

    // --- イベント駆動：呼ぶと一瞬強くして自動で戻る ---
    void Trigger(float peakIntensity, float durationSec);

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    struct RadialBlurParam {
        Vector2 center;
        float   blurWidth;
        float   intensity;
        int32_t numSamples;
        float   focusRadius;
        float   focusSoftness;
        float   aspect;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    RadialBlurParam* paramData_ = nullptr;

    // エンベロープ（イベント駆動用）
    bool  envelopeActive_ = false;
    float envelopeTime_ = 0.0f;
    float envelopeDuration_ = 0.0f;
    float envelopePeak_ = 0.0f;
    float baseIntensity_ = 0.0f; // 常時掛けのレベル（ImGui/Setで操作）
};