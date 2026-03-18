#include "Skybox.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "utility/Logger.h"

void Skybox::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath) {

	dxCommon_ = dxCommon;
	textureFilePath_ = textureFilePath;

	CreateMesh();

	TextureManager::GetInstance()->LoadTexture(textureFilePath_);

	CreateGraphicsPipeline();

	constantBufferResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	constantBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Skybox::Draw(const Camera& camera) {
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	TransformationMatrix wvpData;
	Matrix4x4 worldMatrix = Matrix4x4::Translate(camera.GetTranslate());
	wvpData.WVP = worldMatrix * camera.GetViewProjectionMatrix();
	*constantBufferData_ = wvpData;

	commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource_->GetGPUVirtualAddress());
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_);
	commandList->SetGraphicsRootDescriptorTable(1, srvHandle);

	commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void Skybox::CreateMesh() {
	Vertex vertices[24]; // 6麺 * 4頂点

	// 右面。描画インデックスは[0,1,2][2,1,3]で内側を向く
	vertices[0].position = { 1.0f,1.0f,1.0f,1.0f };
	vertices[1].position = { 1.0f,1.0f,-1.0f,1.0f };
	vertices[2].position = { 1.0f,-1.0f,1.0f,1.0f };
	vertices[3].position = { 1.0f,-1.0f,-1.0f,1.0f };
	// 左面。描画インデックスは[4,5,6][6,5,7]
	vertices[4].position = { -1.0f,1.0f,-1.0f,1.0f };
	vertices[5].position = { -1.0f,1.0f,1.0f,1.0f };
	vertices[6].position = { -1.0f,-1.0f,-1.0f,1.0f };
	vertices[7].position = { -1.0f,-1.0f,1.0f,1.0f };
	// 前面。描画インデックスは[8,9,10][10,9,11]
	vertices[8].position = { -1.0f,1.0f,1.0f,1.0f };
	vertices[9].position = { 1.0f,1.0f,1.0f,1.0f };
	vertices[10].position = { -1.0f,-1.0f,1.0f,1.0f };
	vertices[11].position = { 1.0f,-1.0f,1.0f,1.0f };
	// 背面。描画インデックスは[12,13,14][14,13,15]
	vertices[12].position = { 1.0f, 1.0f, -1.0f, 1.0f };
	vertices[13].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertices[14].position = { 1.0f, -1.0f, -1.0f, 1.0f };
	vertices[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	// 上面。描画インデックスは[16,17,18][18,17,19]
	vertices[16].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertices[17].position = { 1.0f, 1.0f, -1.0f, 1.0f };
	vertices[18].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertices[19].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 下面。描画インデックスは[20,21,22][22,21,23]
	vertices[20].position = { -1.0f, -1.0f, 1.0f, 1.0f };
	vertices[21].position = { 1.0f, -1.0f, 1.0f, 1.0f };
	vertices[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertices[23].position = { 1.0f, -1.0f, -1.0f, 1.0f };

	uint16_t indices[] = {
		 0, 1, 2,  2, 1, 3, // 右
		 4, 5, 6,  6, 5, 7, // 左
		 8, 9, 10, 10, 9, 11, // 前
		12, 13, 14, 14, 13, 15, // 後
		16, 17, 18, 18, 17, 19, // 上
		20, 21, 22, 22, 21, 23  // 下
	};

	vertexBufferResource_ = dxCommon_->CreateBufferResource(sizeof(vertices));
	Vertex* vertexData = nullptr;
	vertexBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices, sizeof(vertices));
	vertexBufferResource_->Unmap(0, nullptr);

	vertexBufferView_.BufferLocation = vertexBufferResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(vertices);
	vertexBufferView_.StrideInBytes = sizeof(Vertex);

	indexBufferResource_ = dxCommon_->CreateBufferResource(sizeof(indices));

	uint16_t* indexData = nullptr;
	indexBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, indices, sizeof(indices));
	indexBufferResource_->Unmap(0, nullptr);

	indexBufferView_.BufferLocation = indexBufferResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(indices);
	indexBufferView_.Format = DXGI_FORMAT_R16_UINT;

}

void Skybox::CreateGraphicsPipeline() {

	/* RootParameterの設定 */
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	// b0: TransformationMatrix(WVP行列)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// t0: TextureCube(DescriptorTable)
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	/* StaticSamplerの設定 (s0: Sampler) */
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	/* RootSignature作成 */
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pStaticSamplers = staticSamplers;
	rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	ID3D12Device* device = dxCommon_->GetDevice();
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	/* PSO */

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	psoDesc.InputLayout = { inputElementDescs,_countof(inputElementDescs) };

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skybox.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skybox.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 全ピクセルがz=1に出力されるので、わざわざ書き込む必要がない
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

}
