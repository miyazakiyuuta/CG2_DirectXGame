#include "debug/debugSphere.h"

#include <vector>
#include <numbers>

void DebugSphere::Initialize(DirectXCommon* dxCommon) {

	dxCommon_ = dxCommon;
	auto device = dxCommon_->GetDevice();

	// ジオメトリ生成
	const uint32_t kSubdivision = 16; // 16分割
	std::vector<Vertex> vertices;
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
		float theta = std::numbers::pi_v<float> * lat / kSubdivision;
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
			float phi = 2.0f * std::numbers::pi_v<float> *lon / kSubdivision;
			Vertex v;
			v.pos.x = std::sin(theta) * std::cos(phi);
			v.pos.y = std::cos(theta);
			v.pos.z = std::sin(theta) * std::sin(phi);
			vertices.push_back(v);
		}
	}

	// インデックス生成 (ワイヤーフレーム用 LineList)
	std::vector<uint32_t> indices;
	for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
		for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
			uint32_t start = lat * (kSubdivision + 1) + lon;
			// 横の線
			indices.push_back(start); indices.push_back(start + 1);
			// 縦の線
			indices.push_back(start); indices.push_back(start + (kSubdivision + 1));
		}
	}
	indexCount_ = static_cast<uint32_t>(indices.size());

	// バッファの生成 (DirectXCommon::CreateBufferResourceを利用)
	vertexBufferResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
	Vertex* vData = nullptr;
	vertexBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&vData));
	std::memcpy(vData, vertices.data(), sizeof(Vertex) * vertices.size());
	vertexBufferResource_->Unmap(0, nullptr);

	vertexBufferView_.BufferLocation = vertexBufferResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(Vertex) * (uint32_t)vertices.size();
	vertexBufferView_.StrideInBytes = sizeof(Vertex);

	indexBufferResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
	uint32_t* iData = nullptr;
	indexBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&iData));
	std::memcpy(iData, indices.data(), sizeof(uint32_t) * indices.size());
	indexBufferResource_->Unmap(0, nullptr);

	indexBufferView_.BufferLocation = indexBufferResource_->GetGPUVirtualAddress();
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * (uint32_t)indices.size();

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

	// 定数バッファの生成 (256byteアラインメント)
	const UINT cbSize = (sizeof(ConstBufferData) + 0xFF) & ~0xFF;
	constantBufferResource_ = dxCommon_->CreateBufferResource(cbSize);
	constantBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstantData_));

}

void DebugSphere::Draw(const std::vector<Vector3>& centers, float radius, const Vector4& color, const Camera& camera) {
	if (centers.empty())return;
	
	auto commandList = dxCommon_->GetCommandList();
	// 描画する個数(最大128個まで)
	uint32_t instanceCount = static_cast<uint32_t>((std::min)((size_t)128, centers.size()));

	for (uint32_t i = 0; i < instanceCount; ++i) {
		Matrix4x4 matWorld = Matrix4x4::Scale({ radius,radius,radius }) * Matrix4x4::Translate(centers[i]);
		mappedConstantData_->data[i].matWVP = matWorld * camera.GetViewProjectionMatrix();
		mappedConstantData_->data[i].color = color;
	}

	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	// 定数バッファの転送とセット
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource_->GetGPUVirtualAddress());

	commandList->DrawIndexedInstanced(indexCount_, instanceCount, 0, 0, 0);
}
