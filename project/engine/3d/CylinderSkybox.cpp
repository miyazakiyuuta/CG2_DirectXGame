#include "CylinderSkybox.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include "2d/TextureManager.h"
#include "utility/Logger.h"
#include <numbers>

using namespace Logger;

void CylinderSkybox::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& texturePath) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(texturePath);

	transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));

	materialResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialForGPU));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->uvTransform = Matrix4x4::Identity();

	CreateVertexResource();
	CreateGraphicsPipelineState();
}

void CylinderSkybox::Update() {
	assert(camera_);

	Matrix4x4 world = Matrix4x4::Scale(transform_.scale)
		* Matrix4x4::Rotate(transform_.rotate)
		* Matrix4x4::Translate(transform_.translate);

	transformData_->world = world;
	transformData_->wvp = world * camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
}

void CylinderSkybox::Draw() {
	auto* cmd = dxCommon_->GetCommandList();

	cmd->SetGraphicsRootSignature(rootSignature_.Get());
	cmd->SetPipelineState(pipelineState_.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vertexBufferView_);

	cmd->SetGraphicsRootConstantBufferView(0, transformResource_->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(textureSrvIndex_));

	cmd->DrawInstanced(vertexCount_, 1, 0, 0);
}

void CylinderSkybox::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
    srvRange[0].BaseShaderRegister = 0;
    srvRange[0].NumDescriptors = 1;
    srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[3] = {};
    // [0] b0 VS: TransformationMatrix
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[0].Descriptor.ShaderRegister = 0;
    // [1] b0 PS: Material
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].Descriptor.ShaderRegister = 0;
    // [2] t0 PS: Texture
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[2].DescriptorTable.pDescriptorRanges = srvRange;
    params[2].DescriptorTable.NumDescriptorRanges = _countof(srvRange);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = params;
    desc.NumParameters = _countof(params);
    desc.pStaticSamplers = &sampler;
    desc.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) { Log(reinterpret_cast<char*>(error->GetBufferPointer())); assert(false); }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void CylinderSkybox::CreateGraphicsPipelineState() {
    CreateRootSignature();

    D3D12_INPUT_ELEMENT_DESC inputElements[3] = {};
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[2].SemanticName = "NORMAL";
    inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE; // 背景なので不透明

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; // ← 内側を描画するキモ
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = true;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 背景は深度書き込みしない
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    auto vs = dxCommon_->CompileShader(L"resources/shaders/CylinderSkybox.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"resources/shaders/CylinderSkybox.PS.hlsl", L"ps_6_0");
    assert(vs && ps);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void CylinderSkybox::CreateVertexResource() {
    const uint32_t kDivide = 32;
    vertexCount_ = kDivide * 6;

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertexCount_);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * vertexCount_;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    const float kRadius = 1.0f;
    const float kHeight = 3.0f;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kDivide);

    std::vector<VertexData> vertices;
    for (uint32_t i = 0; i < kDivide; ++i) {
        float s0 = std::sin(i * radianPerDivide);
        float c0 = std::cos(i * radianPerDivide);
        float s1 = std::sin((i + 1) * radianPerDivide);
        float c1 = std::cos((i + 1) * radianPerDivide);

        // U反転：内側から見ると左右が鏡像になるため
        float u0 = 1.0f - float(i) / float(kDivide);
        float u1 = 1.0f - float(i + 1) / float(kDivide);

        // 法線を内向きに（外向き法線の符号を反転）
        Vector3 n0 = { s0, 0.0f, -c0 };
        Vector3 n1 = { s1, 0.0f, -c1 };

        Vector4 top0 = { -s0 * kRadius, kHeight, c0 * kRadius, 1.0f };
        Vector4 top1 = { -s1 * kRadius, kHeight, c1 * kRadius, 1.0f };
        Vector4 bottom0 = { -s0 * kRadius, 0.0f,   c0 * kRadius, 1.0f };
        Vector4 bottom1 = { -s1 * kRadius, 0.0f,   c1 * kRadius, 1.0f };

        vertices.push_back({ top0,    { u0, 0.0f }, n0 });
        vertices.push_back({ top1,    { u1, 0.0f }, n1 });
        vertices.push_back({ bottom0, { u0, 1.0f }, n0 });
        vertices.push_back({ bottom0, { u0, 1.0f }, n0 });
        vertices.push_back({ top1,    { u1, 0.0f }, n1 });
        vertices.push_back({ bottom1, { u1, 1.0f }, n1 });
    }

    VertexData* mapped = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    std::memcpy(mapped, vertices.data(), sizeof(VertexData) * vertexCount_);
    vertexResource_->Unmap(0, nullptr);
}
