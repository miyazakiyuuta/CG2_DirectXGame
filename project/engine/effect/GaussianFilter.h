#pragma once
#include "effect/ConvolutionEffect.h"
#include "base/RenderTarget.h"

#include <memory>
#include <cstdint>

// 分離可能（2パス）GaussianFilter。横パス→中間RT→縦パス。
// K（半径）とσをImGuiで実時間調整できる。
class GaussianFilter : public ConvolutionEffect {
public:
    GaussianFilter() { name = "GaussianFilter"; }

    // 中間RTが使うRTVスロット番号。AddEffectの前に必ず設定する。
    // 既存：0,1=swapchain / 2=scene / 3,4=pingPong → 空きは5以降。
    void SetIntermediateRtvIndex(uint32_t index) { intermediateRtvIndex_ = index; }

    bool     IsSeparable() const override { return true; }
    uint32_t RenderFirstPass(uint32_t srcSrvIndex) override; // 横
    void     Draw(uint32_t srcSrvIndex) override;            // 縦
    void     DrawImGui() override;

protected:
    const wchar_t* GetPixelShaderPath() const override {
        return L"resources/shaders/GaussianFilter.PS.hlsl";
    }
    void InitializeParam() override;

private:
    // HLSL側 GaussianParam と一致（16byte）
    struct GaussianParam {
        int32_t kernelRadius;
        float   sigma;
        float   intensity;
        int32_t direction; // 0:横 1:縦
    };

    void UpdateParams(); // 両CBufferへ現在のK/σ/intensityを書き込む

    // 横・縦で direction だけ違う2本。使い回すと両パス同方向になるため分ける。
    Microsoft::WRL::ComPtr<ID3D12Resource> paramH_ = nullptr; // direction=0
    Microsoft::WRL::ComPtr<ID3D12Resource> paramV_ = nullptr; // direction=1
    GaussianParam* mappedH_ = nullptr;
    GaussianParam* mappedV_ = nullptr;

    std::unique_ptr<RenderTarget> intermediate_; // 横の結果（縦の入力）
    uint32_t intermediateRtvIndex_ = 5;

    // ImGuiで編集するCPU側の値
    int   kernelRadius_ = 3;    // 7x7
    float sigma_ = 4.0f;
    float intensity_ = 1.0f;
};