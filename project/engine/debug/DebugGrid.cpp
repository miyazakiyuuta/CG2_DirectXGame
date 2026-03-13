#include "DebugGrid.h"

void DebugGrid::Initialize(DirectXCommon* dxCommon, float gridSize, uint32_t subdivision) {
	dxCommon_ = dxCommon;
	auto device = dxCommon_->GetDevice();
	const float kGridSize = gridSize;
	const int kSubdivision = subdivision;
	const float kStep = kGridSize / kSubdivision;

	std::vector<Vertex> vertices;
	// X軸方向の線(Z軸と平行な線)
	for (int i = -kSubdivision; i <= (int)kSubdivision; ++i) {
		float pos = i * kStep;
		vertices.push_back({ Vector3{pos,0.0f,-kGridSize} }); // 始点
		vertices.push_back({ Vector3{pos,0.0f,kGridSize} }); // 終点
	}
	// Z軸の方向の線(X軸と平行な線)
	for (int i = -kSubdivision; i <= (int)kSubdivision; ++i) {
		float pos = i * kStep;
		vertices.push_back({ Vector3{-kGridSize,0.0f,pos} });
		vertices.push_back({ Vector3{kGridSize,0.0f,pos} });
	}
	vertexCount_ = static_cast<uint32_t>(vertices.size());

	vertexBufferResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
	Vertex* vData = nullptr;
	vertexBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&vData));
	std::memcpy(vData, vertices.data(), sizeof(Vertex) * vertices.size());
	vertexBufferResource_->Unmap(0, nullptr);

	vertexBufferView_.BufferLocation = vertexBufferResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(Vertex) * (uint32_t)vertices.size();
	vertexBufferView_.StrideInBytes = sizeof(Vertex);

	const UINT cbSize = (sizeof(ConstBufferData) + 0xFF) & ~0xFF;
	constantBufferResource_ = dxCommon_->CreateBufferResource(cbSize);
	constantBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstantData_));

	/* RootSignature & PSO */

	// RootSignatureの作成
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファ
	rootParameters[0].Descriptor.ShaderRegister = 0; // register(b0)
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	// シェーダーコンパイル
	IDxcBlob* vsBlob = dxCommon_->CompileShader(L"resources/shaders/DebugSphere.VS.hlsl", L"vs_6_0");
	IDxcBlob* psBlob = dxCommon_->CompileShader(L"resources/shaders/DebugSphere.PS.hlsl", L"ps_6_0");

	// PSOの作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();

	// インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };

	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

	// ラスタライザ設定 (ワイヤーフレーム)
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// ブレンド・深度設定
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;

	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}

void DebugGrid::Draw(const Camera& camera) {
	auto commandList = dxCommon_->GetCommandList();

	mappedConstantData_->matWVP = camera.GetViewProjectionMatrix();
	mappedConstantData_->color = { 0.5f,0.5f,0.5f,1.0f }; // グレー

	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource_->GetGPUVirtualAddress());

	commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}
