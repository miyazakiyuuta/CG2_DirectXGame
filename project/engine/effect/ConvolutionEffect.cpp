#include "ConvolutionEffect.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "utility/Logger.h"
#include <cassert>

using namespace Logger;

void ConvolutionEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateGraphicsPipelineState(); // 中でCreateRootSignature + GetPixelShaderPath()を使う
    InitializeParam();             // 派生クラスがCBufferを作って初期値を入れる
}

void ConvolutionEffect::CreateParamBuffer(size_t size) {
    paramResource_ = dxCommon_->CreateBufferResource(size);
    paramResource_->Map(0, nullptr, &mappedParam_);
}

void ConvolutionEffect::Draw(uint32_t srcSrvIndex) {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // root param 0 : 入力テクスチャ(SRV) / root param 1 : CBuffer(b0)
    srvManager_->SetGraphicsRootDescriptorTable(0, srcSrvIndex);
    commandList->SetGraphicsRootConstantBufferView(1, paramResource_->GetGPUVirtualAddress());

    commandList->DrawInstanced(3, 1, 0, 0); // フルスクリーン三角形
}

void ConvolutionEffect::CreateRootSignature() {
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

    // CLAMPサンプラ：端の処理を「端の値を延長」にする（追加実装ゼロ）
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

void ConvolutionEffect::CreateGraphicsPipelineState() {
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

    // VSはFullscreen（共通）、PSは派生クラスが指定するもの
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(GetPixelShaderPath(), L"ps_6_0");
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
