// effect/DepthBasedOutline.cpp
#include "DepthBasedOutline.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include "utility/Logger.h"
#include <cassert>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
using namespace Logger;

void DepthBasedOutline::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    CreateGraphicsPipelineState(); // 内部でCreateRootSignature

    paramResource_ = dxCommon_->CreateBufferResource(sizeof(OutlineParam));
    paramResource_->Map(0, nullptr, reinterpret_cast<void**>(&paramData_));
    paramData_->projectionInverse = Matrix4x4::Identity();
    paramData_->lineColor = lineColor_;
    paramData_->strength = strength_;
    paramData_->threshold = threshold_;
    paramData_->debugView = debugView_;
    paramData_->pad0 = paramData_->pad1 = 0.0f;

    // 深度SRV（手順2で追加したメソッド）
    depthSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateDepthSrv(depthSrvIndex_, dxCommon_->GetDepthStencilResource());
}

void DepthBasedOutline::Update(float /*deltaTime*/) {
    if (camera_) {
        paramData_->projectionInverse = camera_->GetProjectionMatrix().Inverse();
    }
    paramData_->lineColor = lineColor_;
    paramData_->strength = strength_;
    paramData_->threshold = threshold_;
    paramData_->debugView = debugView_;
}

void DepthBasedOutline::SetProjectionInverse(const Matrix4x4& m) {
    if (paramData_) { paramData_->projectionInverse = m; }
}

void DepthBasedOutline::Draw(uint32_t srcSrvIndex) {
    auto cl = dxCommon_->GetCommandList();
    ID3D12Resource* depth = dxCommon_->GetDepthStencilResource();

    // 深度を読める状態へ
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = depth;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cl->ResourceBarrier(1, &b);

    cl->SetGraphicsRootSignature(rootSignature_.Get());
    cl->SetPipelineState(pipelineState_.Get());
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    srvManager_->SetGraphicsRootDescriptorTable(0, srcSrvIndex);    // t0 カラー
    srvManager_->SetGraphicsRootDescriptorTable(1, depthSrvIndex_); // t1 深度
    cl->SetGraphicsRootConstantBufferView(2, paramResource_->GetGPUVirtualAddress()); // b0
    cl->DrawInstanced(3, 1, 0, 0);

    // 次フレームの深度書き込みのため戻す
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    cl->ResourceBarrier(1, &b);
}

void DepthBasedOutline::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::ColorEdit3("Line Color", &lineColor_.x);
    ImGui::SliderFloat("Strength", &strength_, 0.0f, 30.0f);
    ImGui::SliderFloat("Threshold", &threshold_, 0.0f, 1.0f);
    bool dbg = debugView_ != 0;
    if (ImGui::Checkbox("Debug (edge weight)", &dbg)) { debugView_ = dbg ? 1 : 0; }
#endif
}

void DepthBasedOutline::OnResize() {
    if (!srvManager_ || !dxCommon_) { return; }
    // 同じスロットに、今の深度リソースを指すSRVを作り直す（Allocateし直さない）
    srvManager_->CreateDepthSrv(depthSrvIndex_, dxCommon_->GetDepthStencilResource());
}

void DepthBasedOutline::CreateRootSignature() {
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    D3D12_DESCRIPTOR_RANGE colorRange{};
    colorRange.BaseShaderRegister = 0; colorRange.NumDescriptors = 1; // t0
    colorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    colorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE depthRange{};
    depthRange.BaseShaderRegister = 1; depthRange.NumDescriptors = 1; // t1
    depthRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    depthRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER p[3] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[0].DescriptorTable.pDescriptorRanges = &colorRange;
    p[0].DescriptorTable.NumDescriptorRanges = 1;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].DescriptorTable.pDescriptorRanges = &depthRange;
    p[1].DescriptorTable.NumDescriptorRanges = 1;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[2].Descriptor.ShaderRegister = 0; // b0

    D3D12_STATIC_SAMPLER_DESC s[2] = {};
    s[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;           // s0 カラー
    s[0].AddressU = s[0].AddressV = s[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s[0].MaxLOD = D3D12_FLOAT32_MAX; s[0].ShaderRegister = 0;
    s[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    s[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;            // s1 深度
    s[1].AddressU = s[1].AddressV = s[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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

void DepthBasedOutline::CreateGraphicsPipelineState() {
    CreateRootSignature();

    D3D12_INPUT_LAYOUT_DESC inputLayout{}; // 頂点バッファ無し
    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0].BlendEnable = FALSE;
    D3D12_RASTERIZER_DESC raster{};
    raster.CullMode = D3D12_CULL_MODE_NONE; raster.FillMode = D3D12_FILL_MODE_SOLID;

    auto vs = dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"resources/shaders/DepthBasedOutline.PS.hlsl", L"ps_6_0");
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