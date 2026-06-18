#include "RadialBlur.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "utility/Logger.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Logger;

void RadialBlur::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    CreateGraphicsPipelineState();

    paramResource_ = dxCommon_->CreateBufferResource(sizeof(RadialBlurParam));
    paramResource_->Map(0, nullptr, reinterpret_cast<void**>(&paramData_));

    // デフォルト：中心固定・控えめ
    paramData_->center = { 0.5f, 0.5f };
    paramData_->blurWidth = 0.012f;
    paramData_->intensity = 0.0f;  // 内部計算で上書きされる
    paramData_->numSamples = 10;    // 資料と同じ
    paramData_->focusRadius = 0.18f; // 中心の鮮明領域
    paramData_->focusSoftness = 0.35f;
    paramData_->aspect = 1.0f;

    baseIntensity_ = 0.0f; // 既定はOFF。見たい時はImGuiで上げる or Trigger
}

void RadialBlur::Update(float deltaTime) {
    if (envelopeActive_) {
        envelopeTime_ += deltaTime;
        float t = envelopeTime_ / envelopeDuration_; // 0..1
        if (t >= 1.0f) {
            envelopeActive_ = false;
            paramData_->intensity = baseIntensity_;
        } else {
            // 立ち上がり一瞬→イーズアウトで減衰（被弾/爆発のパンチ感）
            float env = 1.0f - t;
            env *= env;
            paramData_->intensity = baseIntensity_ + (envelopePeak_ - baseIntensity_) * env;
        }
    } else {
        paramData_->intensity = baseIntensity_;
    }
}

void RadialBlur::Trigger(float peakIntensity, float durationSec) {
    envelopePeak_ = peakIntensity;
    envelopeDuration_ = (durationSec > 0.0001f) ? durationSec : 0.0001f;
    envelopeTime_ = 0.0f;
    envelopeActive_ = true;
}

void RadialBlur::SetCenterWorld(const Vector3& world, const Matrix4x4& viewProj) {
    // ※ row-vector / row-major（KamataEngine系）を想定: clip = (world,1) * viewProj
    //    行列規約が違う場合はここの掛け方を調整してください。
    const auto& m = viewProj.m; // float m[4][4] を想定
    float cx = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0];
    float cy = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1];
    float cw = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3];

    if (cw <= 0.0001f) return; // カメラ背後は無視（前回値を維持）

    float ndcX = cx / cw;
    float ndcY = cy / cw;
    paramData_->center = { ndcX * 0.5f + 0.5f, -ndcY * 0.5f + 0.5f }; // Yは反転
}

void RadialBlur::Draw(uint32_t srcSrvIndex) {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    srvManager_->SetGraphicsRootDescriptorTable(0, srcSrvIndex);
    commandList->SetGraphicsRootConstantBufferView(1, paramResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}

void RadialBlur::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::DragFloat2("Center (UV)", &paramData_->center.x, 0.005f, 0.0f, 1.0f);
    ImGui::SliderFloat("BlurWidth", &paramData_->blurWidth, 0.0f, 0.05f);
    ImGui::SliderInt("NumSamples", &paramData_->numSamples, 1, 64);
    ImGui::SliderFloat("Intensity (base)", &baseIntensity_, 0.0f, 1.0f);
    ImGui::SliderFloat("FocusRadius", &paramData_->focusRadius, 0.0f, 1.0f);
    ImGui::SliderFloat("FocusSoftness", &paramData_->focusSoftness, 0.001f, 1.0f);
    ImGui::SliderFloat("Aspect", &paramData_->aspect, 0.5f, 2.0f);
    if (ImGui::Button("Trigger (test)")) {
        Trigger(1.0f, 1.4f);
    }
#endif
}

void RadialBlur::CreateRootSignature() {
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
    srvRange[0].BaseShaderRegister = 0; // t0
    srvRange[0].NumDescriptors = 1;
    srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = srvRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(srvRange);
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0; // b0

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    ID3D12Device* device = dxCommon_->GetDevice();
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void RadialBlur::CreateGraphicsPipelineState() {
    CreateRootSignature();

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = nullptr;
    inputLayoutDesc.NumElements = 0;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(L"resources/shaders/RadialBlur.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = FALSE;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ID3D12Device* device = dxCommon_->GetDevice();
    HRESULT hr = device->CreateGraphicsPipelineState(
        &pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}