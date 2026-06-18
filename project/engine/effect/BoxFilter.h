#pragma once
#include "effect/ConvolutionEffect.h"

#include <cstdint>

// 3x3 / 5x5 ... を実行時に切り替えられるBoxFilter（Smoothing）。
// 重みは全て等しい（=平均）ので、シェーダ側はKだけ受け取れば足りる。
class BoxFilter : public ConvolutionEffect {
public:
    BoxFilter() { name = "BoxFilter"; }

    void DrawImGui() override;

    void SetKernelRadius(int radius) { param_->kernelRadius = radius; }
    void SetIntensity(float intensity) { param_->intensity = intensity; }

protected:
    const wchar_t* GetPixelShaderPath() const override {
        return L"resources/shaders/BoxFilter.PS.hlsl";
    }
    void InitializeParam() override;

private:
    // HLSL側のcbufferと一致させる。
    // int(4) + float(4) = 8byte。16byte境界に合わせるためpaddingを2つ足す。
    struct BoxFilterParam {
        int32_t kernelRadius;
        float intensity;
        float pad0;
        float pad1;
    };

    BoxFilterParam* param_ = nullptr;
};
