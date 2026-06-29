// effect/Dissolve.h
#pragma once
#include "effect/IPostEffect.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>

// シーン全体に対するDissolve（溶解）ポストエフェクト。
// maskノイズの値がthreshold以下の画素を「穴」とみなしholeColorで塗り、
// 穴のすぐ外側(threshold〜threshold+edgeWidth)にedgeColorの縁取りを加算する。
class Dissolve : public IPostEffect {
public:
    Dissolve() { name = "Dissolve"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Update(float deltaTime) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    // HLSL側と一致（float3+floatで各16byteに収め、計48byte）
    struct DissolveParam {
        Vector3 edgeColor;   // 12  溶けぎわの発光色
        float   threshold;   //  4  溶けの進行度(0..1)
        Vector3 holeColor;   // 12  溶けた穴を塗る色
        float   edgeWidth;   //  4  縁取りのなじみ幅(smoothstepの幅)
        Vector2 maskTiling;  //  8  maskの繰り返し倍率
        float   pad0;        //  4
        float   pad1;        //  4
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager*    srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource>      paramResource_;
    DissolveParam* paramData_ = nullptr;

    // maskノイズ（2種をロードしImGuiで切替）
    uint32_t maskSrvIndices_[2] = { 0, 0 };
    int      maskIndex_ = 0; // 0=noise0, 1=noise1

    // ImGui調整値（既定は資料に寄せる：穴=緑、縁=オレンジ）
    Vector3 edgeColor_  = { 1.0f, 0.4f, 0.3f };
    Vector3 holeColor_  = { 0.0f, 1.0f, 0.0f };
    float   edgeWidth_  = 0.03f;
    Vector2 maskTiling_ = { 1.0f, 1.0f };

    // 時間駆動アニメ（threshold = time_/duration_）
    float threshold_ = 0.0f;
    float duration_  = 2.0f;
    float time_      = 0.0f;
    bool  playing_   = true;
    bool  loop_      = true;
    bool  reverse_   = false;
};
