// effect/Noise.h
#pragma once
#include "effect/IPostEffect.h"

#include <wrl/client.h>
#include <d3d12.h>

// GPUで乱数を生成するノイズ・ポストエフェクト。
// Pureモード = グレースケール乱数をそのまま出力（資料01-07の確認用）、
// Grainモード = 0.5中心の符号付きノイズをシーン色へ加算（フィルムグレイン）。
// 経過時間をSeedに加算して毎フレーム模様を変化させる（Freezeで停止＝固定すれば同じ結果）。
class Noise : public IPostEffect {
public:
    Noise() { name = "Noise"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Update(float deltaTime) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

    void SetMode(int mode) { mode_ = mode; }
    void SetIntensity(float v) { intensity_ = v; }
    void SetSpeed(float v) { speed_ = v; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    // HLSL側と一致（float,float,int,float で16byte=1レジスタ）
    struct NoiseParam {
        float time;      // 積算した経過時間
        float intensity; // Grainの加算強度
        int   mode;      // 0=Pure / 1=Grain
        float pad;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    NoiseParam* paramData_ = nullptr;

    // ImGui調整値
    int   mode_      = 0;      // 既定はPure（資料の出力に忠実）
    float intensity_ = 0.15f;  // Grain時の加算強度
    float speed_     = 1.0f;   // 時間の進む速さ
    bool  frozen_    = false;  // trueで時間を止める（固定すれば同じ結果＝デバッグしやすい）
    float time_      = 0.0f;   // 積算した経過時間
};
