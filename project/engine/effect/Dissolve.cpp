// effect/Dissolve.cpp
#include "Dissolve.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "2d/TextureManager.h"
#include "utility/Logger.h"
#include <cassert>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
using namespace Logger;

namespace {
    // maskに使うノイズ画像（ImGuiのComboと添字を一致させる）
    constexpr const char* kMaskPaths[2] = {
        "resources/noise0.png",
        "resources/noise1.png",
    };
    constexpr const char* kMaskNames[2] = { "noise0", "noise1" };
}

void Dissolve::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    CreateGraphicsPipelineState(); // 内部でCreateRootSignature

    paramResource_ = dxCommon_->CreateBufferResource(sizeof(DissolveParam));
    paramResource_->Map(0, nullptr, reinterpret_cast<void**>(&paramData_));
    paramData_->edgeColor  = edgeColor_;
    paramData_->threshold  = threshold_;
    paramData_->holeColor  = holeColor_;
    paramData_->edgeWidth  = edgeWidth_;
    paramData_->maskTiling = maskTiling_;
    paramData_->pad0 = paramData_->pad1 = 0.0f;

    // maskノイズをTextureManager経由でロードし、SRVインデックスを取得（共有SRVヒープに載る）
    for (int i = 0; i < 2; ++i) {
        TextureManager::GetInstance()->LoadTexture(kMaskPaths[i]);
        maskSrvIndices_[i] = TextureManager::GetInstance()->GetSrvIndex(kMaskPaths[i]);
    }
}

void Dissolve::Update(float deltaTime) {
    // 時間でthresholdを0→1に進める（Reverseで逆再生、Loopで巻き戻し）
    if (playing_) {
        time_ += reverse_ ? -deltaTime : deltaTime;
        if (loop_) {
            if (time_ > duration_) { time_ -= duration_; }
            if (time_ < 0.0f)      { time_ += duration_; }
        } else {
            if (time_ > duration_) { time_ = duration_; playing_ = false; }
            if (time_ < 0.0f)      { time_ = 0.0f;      playing_ = false; }
        }
        threshold_ = (duration_ > 0.0f) ? (time_ / duration_) : 0.0f;
    }

    // ImGuiで触った値も含めてCBへ反映
    paramData_->edgeColor  = edgeColor_;
    paramData_->threshold  = threshold_;
    paramData_->holeColor  = holeColor_;
    paramData_->edgeWidth  = edgeWidth_;
    paramData_->maskTiling = maskTiling_;
}

void Dissolve::Draw(uint32_t srcSrvIndex) {
    auto cl = dxCommon_->GetCommandList();
    cl->SetGraphicsRootSignature(rootSignature_.Get());
    cl->SetPipelineState(pipelineState_.Get());
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    srvManager_->SetGraphicsRootDescriptorTable(0, srcSrvIndex);                  // t0 シーン色
    srvManager_->SetGraphicsRootDescriptorTable(1, maskSrvIndices_[maskIndex_]);  // t1 maskノイズ
    cl->SetGraphicsRootConstantBufferView(2, paramResource_->GetGPUVirtualAddress()); // b0
    cl->DrawInstanced(3, 1, 0, 0);
}

void Dissolve::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Combo("Mask", &maskIndex_, kMaskNames, 2);
    ImGui::ColorEdit3("Hole Color", &holeColor_.x);
    ImGui::ColorEdit3("Edge Color", &edgeColor_.x);
    ImGui::SliderFloat("Edge Width", &edgeWidth_, 0.001f, 0.3f);
    ImGui::SliderFloat2("Mask Tiling", &maskTiling_.x, 0.1f, 8.0f);

    ImGui::Separator();
    ImGui::Checkbox("Playing", &playing_); ImGui::SameLine();
    ImGui::Checkbox("Loop", &loop_);       ImGui::SameLine();
    ImGui::Checkbox("Reverse", &reverse_);
    ImGui::SliderFloat("Duration(s)", &duration_, 0.1f, 10.0f);
    if (ImGui::Button("Restart")) {
        time_ = reverse_ ? duration_ : 0.0f;
        playing_ = true;
    }
    // 手動スクラブ：停止して値を直接動かして確認できる（操作を時間にも同期）
    if (ImGui::SliderFloat("Threshold", &threshold_, 0.0f, 1.0f)) {
        time_ = threshold_ * duration_;
    }
#endif
}

void Dissolve::CreateRootSignature() {
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    D3D12_DESCRIPTOR_RANGE colorRange{};
    colorRange.BaseShaderRegister = 0; colorRange.NumDescriptors = 1; // t0
    colorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    colorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE maskRange{};
    maskRange.BaseShaderRegister = 1; maskRange.NumDescriptors = 1; // t1
    maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER p[3] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[0].DescriptorTable.pDescriptorRanges = &colorRange;
    p[0].DescriptorTable.NumDescriptorRanges = 1;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].DescriptorTable.pDescriptorRanges = &maskRange;
    p[1].DescriptorTable.NumDescriptorRanges = 1;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[2].Descriptor.ShaderRegister = 0; // b0

    D3D12_STATIC_SAMPLER_DESC s[2] = {};
    s[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // s0 シーン色 Linear/Clamp
    s[0].AddressU = s[0].AddressV = s[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s[0].MaxLOD = D3D12_FLOAT32_MAX; s[0].ShaderRegister = 0;
    s[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    s[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // s1 mask Linear/Wrap（タイル可能に）
    s[1].AddressU = s[1].AddressV = s[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s[1].MaxLOD = D3D12_FLOAT32_MAX; s[1].ShaderRegister = 1;
    s[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    desc.pParameters = p;        desc.NumParameters = _countof(p);
    desc.pStaticSamplers = s;    desc.NumStaticSamplers = _countof(s);

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) { Log(reinterpret_cast<char*>(err->GetBufferPointer())); assert(false); }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(),
        sig->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void Dissolve::CreateGraphicsPipelineState() {
    CreateRootSignature();

    D3D12_INPUT_LAYOUT_DESC inputLayout{}; // 頂点バッファ無し
    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0].BlendEnable = FALSE;
    D3D12_RASTERIZER_DESC raster{};
    raster.CullMode = D3D12_CULL_MODE_NONE; raster.FillMode = D3D12_FILL_MODE_SOLID;

    auto vs = dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"resources/shaders/Dissolve.PS.hlsl", L"ps_6_0");
    assert(vs && ps);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.InputLayout = inputLayout;
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.BlendState = blend;
    pso.RasterizerState = raster;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    D3D12_DEPTH_STENCIL_DESC ds{}; ds.DepthEnable = false; ds.StencilEnable = FALSE;
    pso.DepthStencilState = ds;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
