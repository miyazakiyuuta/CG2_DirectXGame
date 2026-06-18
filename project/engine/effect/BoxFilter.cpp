#include "BoxFilter.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void BoxFilter::InitializeParam() {
    CreateParamBuffer(sizeof(BoxFilterParam));
    param_ = reinterpret_cast<BoxFilterParam*>(mappedParam_);

    param_->kernelRadius = 1;   // K=1 → 3x3
    param_->intensity = 1.0f;   // 最初はぼかし全開
    param_->pad0 = 0.0f;
    param_->pad1 = 0.0f;
}

void BoxFilter::DrawImGui() {
#ifdef USE_IMGUI
    // Kを変えると 2K+1 四方の平均になる（0で無効、1で3x3、2で5x5...）
    ImGui::SliderInt("Kernel Radius (K)", &param_->kernelRadius, 0, 5);
    int size = param_->kernelRadius * 2 + 1;
    ImGui::Text("Kernel size: %dx%d", size, size);

    // 元画像とぼかし結果のブレンド率
    ImGui::SliderFloat("Intensity", &param_->intensity, 0.0f, 1.0f);
#endif
}
