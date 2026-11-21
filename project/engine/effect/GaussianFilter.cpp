#include "GaussianFilter.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "base/WinApp.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GaussianFilter::InitializeParam() {
    DirectXCommon* dxCommon = GetDxCommon();

    // baseのCreateParamBufferは使わず、横・縦で2本作る
    paramH_ = dxCommon->CreateBufferResource(sizeof(GaussianParam));
    paramH_->Map(0, nullptr, reinterpret_cast<void**>(&mappedH_));
    paramV_ = dxCommon->CreateBufferResource(sizeof(GaussianParam));
    paramV_->Map(0, nullptr, reinterpret_cast<void**>(&mappedV_));

    mappedH_->direction = 0; // 横
    mappedV_->direction = 1; // 縦
    UpdateParams();          // K/σ/intensityの初期値を両方へ

    // 中間RT（横パスの描画先 兼 縦パスの入力）。RTVスロットは自動採番で確保
    intermediate_ = std::make_unique<RenderTarget>();
    intermediate_->Create(
        dxCommon->GetDevice(), GetSrvManager(),
        dxCommon->GetRTVCPUDescriptorHandle(dxCommon->AllocateRtvIndex()),
        WinApp::kClientWidth, WinApp::kClientHeight);
}

void GaussianFilter::UpdateParams() {
    mappedH_->kernelRadius = kernelRadius_;
    mappedH_->sigma = sigma_;
    mappedH_->intensity = intensity_;
    mappedV_->kernelRadius = kernelRadius_;
    mappedV_->sigma = sigma_;
    mappedV_->intensity = intensity_;
}

uint32_t GaussianFilter::RenderFirstPass(uint32_t srcSrvIndex) {
    UpdateParams(); // このフレームの値を両CBufferへ反映（縦の分もここで済ます）

    ID3D12GraphicsCommandList* cl = GetDxCommon()->GetCommandList();

    intermediate_->BeginRenderNoDepth(cl);
    DrawFullscreen(srcSrvIndex, paramH_->GetGPUVirtualAddress()); // 入力＝前段の出力 / 横
    intermediate_->EndRender(cl);

    return intermediate_->GetSrvIndex(); // 縦パスの入力になる
}

void GaussianFilter::Draw(uint32_t srcSrvIndex) {
    // srcSrvIndex = RenderFirstPassが返した中間RTのSRV。
    // 出力先(ping-pong)はEffectManagerが既にBeginRender済み。
    DrawFullscreen(srcSrvIndex, paramV_->GetGPUVirtualAddress()); // 縦
}

void GaussianFilter::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SliderInt("Kernel Radius (K)", &kernelRadius_, 0, 8);
    int size = kernelRadius_ * 2 + 1;
    // 分離なので 2*(2K+1) サンプル。非分離なら (2K+1)^2。これが資料の「軽さ」の正体。
    ImGui::Text("Kernel: %dx%d  / samples: %d (separable) vs %d (naive)",
                size, size, size * 2, size * size);
    ImGui::SliderFloat("Sigma", &sigma_, 0.1f, 8.0f);
    ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 1.0f);
#endif
}