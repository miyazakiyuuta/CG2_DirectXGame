#include "DebugRenderer.h"
#include "2d/TextureManager.h"
#include "base/WinApp.h"

#include <numbers>

DebugRenderer* DebugRenderer::instance = nullptr;
DebugRenderer* DebugRenderer::GetInstance() {
	if (!instance) { instance = new DebugRenderer(); }
	return instance;
}

void DebugRenderer::Finalize() {
	delete instance;
	instance = nullptr;
}

void DebugRenderer::Initialize(DirectXCommon* dxCommon) {
	dxCommon_ = dxCommon;
	auto device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 0; // t0

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // b0
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // t0
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0; // s0

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"resources/shaders/Debug.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"resources/shaders/Debug.PS.hlsl", L"ps_6_0");

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso3DLine_));

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.DepthStencilState.DepthEnable = TRUE; // 3Dなので深度テストを有効にする
	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso3DSolid_));

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.DepthStencilState.DepthEnable = FALSE; // 2Dは常に前面
	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso2DSolid_));

	CreateSphereMesh();
	CreateBox3DMesh();
	CreateBox2DMesh();

	lineBuffer_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * kMaxLineVertices);
	lineBufferView_.BufferLocation = lineBuffer_->GetGPUVirtualAddress();
	lineBufferView_.SizeInBytes = sizeof(Vertex) * kMaxLineVertices;
	lineBufferView_.StrideInBytes = sizeof(Vertex);

	constBuffer_ = dxCommon_->CreateBufferResource((sizeof(ConstBufferData) + 0xFF) & ~0xFF);
	constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));

}

void DebugRenderer::AddSphere(const Vector3& center, float radius, const Vector4& color) {
	sphereRequests_.push_back({ center,radius,color });
}

void DebugRenderer::AddLine(const Vector3& start, const Vector3& end, const Vector4& color) {
	lineVertices_.push_back({ start,{0,0},color });
	lineVertices_.push_back({ end,{0,0},color });
}

void DebugRenderer::AddGrid(const Vector3& center, float gridSize, uint32_t subdivision, const Vector4& color) {
	const float kGridSize = gridSize;
	const int kSubdivision = subdivision;
	const float kStep = kGridSize / kSubdivision;

	// X軸方向の線(Z軸と平行な線)
	for (int i = -(int)kSubdivision; i <= (int)kSubdivision; ++i) {
		float pos = i * kStep;
		AddLine(center + Vector3{ pos,0,-kGridSize }, center + Vector3{ pos,0,kGridSize }, color);
		AddLine(center + Vector3{ -kGridSize,0, pos }, center + Vector3{ kGridSize,0, pos }, color);
	}
}

void DebugRenderer::AddBox3D(const Vector3& center, const Vector3& size, const Vector4& color) {
	// 8つの頂点の座標を計算
	Vector3 min = { center.x - size.x / 2.0f, center.y - size.y / 2.0f, center.z - size.z / 2.0f };
	Vector3 max = { center.x + size.x / 2.0f, center.y + size.y / 2.0f, center.z + size.z / 2.0f };

	// 底面の4本
	AddLine({ min.x, min.y, min.z }, { max.x, min.y, min.z }, color);
	AddLine({ max.x, min.y, min.z }, { max.x, min.y, max.z }, color);
	AddLine({ max.x, min.y, max.z }, { min.x, min.y, max.z }, color);
	AddLine({ min.x, min.y, max.z }, { min.x, min.y, min.z }, color);

	// 天面の4本
	AddLine({ min.x, max.y, min.z }, { max.x, max.y, min.z }, color);
	AddLine({ max.x, max.y, min.z }, { max.x, max.y, max.z }, color);
	AddLine({ max.x, max.y, max.z }, { min.x, max.y, max.z }, color);
	AddLine({ min.x, max.y, max.z }, { min.x, max.y, min.z }, color);

	// 柱の4本
	AddLine({ min.x, min.y, min.z }, { min.x, max.y, min.z }, color);
	AddLine({ max.x, min.y, min.z }, { max.x, max.y, min.z }, color);
	AddLine({ max.x, min.y, max.z }, { max.x, max.y, max.z }, color);
	AddLine({ min.x, min.y, max.z }, { min.x, max.y, max.z }, color);
}

void DebugRenderer::AddBox3DSolid(const Vector3& center, const Vector3& size, const Vector4& color) {
	box3DSolidRequests_.push_back({ center,size,color });
}

void DebugRenderer::AddBox2D(const Vector2& leftTop, const Vector2& size, const Vector4& color) {
	box2DRequests_.push_back({ leftTop, size, color });
}

void DebugRenderer::RenderAll(const Camera& camera) {
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pso3DLine_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	auto whiteHandle = TextureManager::GetInstance()->GetSrvHandleGPU("white");
	commandList->SetGraphicsRootDescriptorTable(1, whiteHandle);

	uint32_t usedCount = 0;

	commandList->SetGraphicsRootConstantBufferView(0, constBuffer_->GetGPUVirtualAddress());

	if (!sphereRequests_.empty()) {
		commandList->IASetVertexBuffers(0, 1, &sphereMesh_.vertexBufferView);
		commandList->IASetIndexBuffer(&sphereMesh_.indexBufferView);

		for (const auto& request : sphereRequests_) {
			if (usedCount >= 128) break;

			Matrix4x4 matWorld =
				Matrix4x4::Scale({ request.radius,request.radius ,request.radius }) *
				Matrix4x4::Translate(request.center);

			mappedData_->data[usedCount].matWVP = matWorld * camera.GetViewProjectionMatrix();
			mappedData_->data[usedCount].color = request.color;

			D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = constBuffer_->GetGPUVirtualAddress() + (sizeof(ConstBufferData::TransformationMatrix) * usedCount);
			commandList->SetGraphicsRootConstantBufferView(0, cbvAddr);
			commandList->DrawIndexedInstanced(sphereMesh_.indexCount, 1, 0, 0, 0);

			usedCount++;
		}
		sphereRequests_.clear();
	}

	if (!lineVertices_.empty()) {
		if (usedCount < 128) {
			Vertex* vertexData = nullptr;
			lineBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
			std::memcpy(vertexData, lineVertices_.data(), sizeof(Vertex) * lineVertices_.size());
			lineBuffer_->Unmap(0, nullptr);

			mappedData_->data[usedCount].matWVP = camera.GetViewProjectionMatrix();
			mappedData_->data[usedCount].color = { 1.0f,1.0f,1.0f,1.0f };

			commandList->SetPipelineState(pso3DLine_.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			commandList->IASetVertexBuffers(0, 1, &lineBufferView_);

			D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = constBuffer_->GetGPUVirtualAddress() + (sizeof(ConstBufferData::TransformationMatrix) * usedCount);
			commandList->SetGraphicsRootConstantBufferView(0, cbvAddr);
			commandList->DrawInstanced((UINT)lineVertices_.size(), 1, 0, 0);

			usedCount++;
		}
		lineVertices_.clear();
	}

	if (!box3DSolidRequests_.empty()) {
		commandList->SetPipelineState(pso3DSolid_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &box3DMesh_.vertexBufferView);
		commandList->IASetIndexBuffer(&box3DMesh_.indexBufferView);

		for (const auto& request : box3DSolidRequests_) {
			if (usedCount >= 128) break;

			Matrix4x4 matWorld = Matrix4x4::Scale(request.size) * Matrix4x4::Translate(request.center);
			mappedData_->data[usedCount].matWVP = matWorld * camera.GetViewProjectionMatrix();
			mappedData_->data[usedCount].color = request.color;

			// 正解の「256バイトずらし」アドレスをセット
			D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = constBuffer_->GetGPUVirtualAddress() + (sizeof(ConstBufferData::TransformationMatrix) * usedCount);
			commandList->SetGraphicsRootConstantBufferView(0, cbvAddr);

			commandList->DrawIndexedInstanced(box3DMesh_.indexCount, 1, 0, 0, 0);
			usedCount++;
		}
		box3DSolidRequests_.clear();
	}

	if (!box2DRequests_.empty()) {
		commandList->SetPipelineState(pso2DSolid_.Get()); // 2D用PSO
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->IASetVertexBuffers(0, 1, &box2DMesh_.vertexBufferView);

		// 2D用の正射影行列 (Screen座標 -> NDC座標)
		// クライアント領域のサイズ (1280x720など) を使用
		float kClientWidth = (float)WinApp::kClientWidth;
		float kClientHeight = (float)WinApp::kClientHeight;

		// 簡易的な2D変換行列 (左上原点)
		Matrix4x4 matProjection = {
			2.0f / kClientWidth,  0.0f,                0.0f, 0.0f,
			0.0f,                -2.0f / kClientHeight, 0.0f, 0.0f,
			0.0f,                 0.0f,                1.0f, 0.0f,
		   -1.0f,                 1.0f,                0.0f, 1.0f
		};

		for (const auto& request : box2DRequests_) {
			if (usedCount >= 128) break;
			// スケールと平行移動
			Matrix4x4 matWorld = Matrix4x4::Scale({ request.size.x, request.size.y, 1.0f }) *
				Matrix4x4::Translate({ request.leftTop.x, request.leftTop.y, 0.0f });

			mappedData_->data[usedCount].matWVP = matWorld * matProjection;
			mappedData_->data[usedCount].color = request.color;

			D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = constBuffer_->GetGPUVirtualAddress() + (sizeof(ConstBufferData::TransformationMatrix) * usedCount);
			commandList->SetGraphicsRootConstantBufferView(0, cbvAddr);
			commandList->DrawInstanced(box2DMesh_.indexCount, 1, 0, 0);

			usedCount++;
		}
		box2DRequests_.clear();
	}

}

void DebugRenderer::CreateSphereMesh() {

	// ジオメトリ(頂点)生成
	const uint32_t kSubdivision = 16; // 16分割
	std::vector<Vertex> vertices;
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
		float theta = std::numbers::pi_v<float> *lat / kSubdivision;
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
			float phi = 2.0f * std::numbers::pi_v<float> *lon / kSubdivision;
			Vertex v;
			v.pos.x = std::sin(theta) * std::cos(phi);
			v.pos.y = std::cos(theta);
			v.pos.z = std::sin(theta) * std::sin(phi);
			v.color = { 1.0f,1.0f,1.0f,1.0f };
			v.uv = { 0.0f,0.0f };
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
	sphereMesh_.indexCount = static_cast<uint32_t>(indices.size());

	// 頂点バッファの生成
	sphereMesh_.vertexBufferResource = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
	Vertex* vertexData = nullptr;
	sphereMesh_.vertexBufferResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(Vertex) * vertices.size());
	sphereMesh_.vertexBufferResource->Unmap(0, nullptr);

	sphereMesh_.vertexBufferView.BufferLocation = sphereMesh_.vertexBufferResource->GetGPUVirtualAddress();
	sphereMesh_.vertexBufferView.SizeInBytes = sizeof(Vertex) * (uint32_t)vertices.size();
	sphereMesh_.vertexBufferView.StrideInBytes = sizeof(Vertex);

	// インデックスバッファ生成
	sphereMesh_.indexBufferResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
	uint32_t* indexData = nullptr;
	sphereMesh_.indexBufferResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
	sphereMesh_.indexBufferResource->Unmap(0, nullptr);

	sphereMesh_.indexBufferView.BufferLocation = sphereMesh_.indexBufferResource->GetGPUVirtualAddress();
	sphereMesh_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	sphereMesh_.indexBufferView.SizeInBytes = sizeof(uint32_t) * (uint32_t)indices.size();

}

void DebugRenderer::CreateBox3DMesh() {
	// 立方体の8頂点 (1x1x1)
	std::vector<Vertex> vertices = {
		{{-0.5f,-0.5f,-0.5f}, {0,0}, {1,1,1,1}}, {{-0.5f, 0.5f,-0.5f}, {0,0}, {1,1,1,1}},
		{{ 0.5f, 0.5f,-0.5f}, {0,0}, {1,1,1,1}}, {{ 0.5f,-0.5f,-0.5f}, {0,0}, {1,1,1,1}},
		{{-0.5f,-0.5f, 0.5f}, {0,0}, {1,1,1,1}}, {{-0.5f, 0.5f, 0.5f}, {0,0}, {1,1,1,1}},
		{{ 0.5f, 0.5f, 0.5f}, {0,0}, {1,1,1,1}}, {{ 0.5f,-0.5f, 0.5f}, {0,0}, {1,1,1,1}},
	};
	// インデックス (三角形リスト)
	std::vector<uint32_t> indices = {
		0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,5,1, 0,4,5,
		1,5,6, 1,6,2, 2,6,7, 2,7,3, 3,7,4, 3,4,0
	};
	box3DMesh_.indexCount = (uint32_t)indices.size();

	// 頂点バッファの生成
	box3DMesh_.vertexBufferResource = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
	Vertex* vertexData = nullptr;
	box3DMesh_.vertexBufferResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(Vertex) * vertices.size());
	box3DMesh_.vertexBufferResource->Unmap(0, nullptr);

	box3DMesh_.vertexBufferView.BufferLocation = box3DMesh_.vertexBufferResource->GetGPUVirtualAddress();
	box3DMesh_.vertexBufferView.SizeInBytes = sizeof(Vertex) * (uint32_t)vertices.size();
	box3DMesh_.vertexBufferView.StrideInBytes = sizeof(Vertex);

	// インデックスバッファ生成
	box3DMesh_.indexBufferResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
	uint32_t* indexData = nullptr;
	box3DMesh_.indexBufferResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
	box3DMesh_.indexBufferResource->Unmap(0, nullptr);

	box3DMesh_.indexBufferView.BufferLocation = box3DMesh_.indexBufferResource->GetGPUVirtualAddress();
	box3DMesh_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	box3DMesh_.indexBufferView.SizeInBytes = sizeof(uint32_t) * (uint32_t)indices.size();
}

void DebugRenderer::CreateBox2DMesh() {
	// 4頂点の TriangleStrip (左上, 右上, 左下, 右下)
	std::vector<Vertex> vertices = {
		{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1,1,1,1} }, // 左上
		{ {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1,1,1,1} }, // 右上
		{ {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1,1,1,1} }, // 左下
		{ {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1,1,1,1} }, // 右下
	};

	box2DMesh_.vertexBufferResource = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
	Vertex* vertexData = nullptr;
	box2DMesh_.vertexBufferResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(Vertex) * vertices.size());
	box2DMesh_.vertexBufferResource->Unmap(0, nullptr);

	box2DMesh_.vertexBufferView.BufferLocation = box2DMesh_.vertexBufferResource->GetGPUVirtualAddress();
	box2DMesh_.vertexBufferView.SizeInBytes = sizeof(Vertex) * (uint32_t)vertices.size();
	box2DMesh_.vertexBufferView.StrideInBytes = sizeof(Vertex);

	box2DMesh_.indexCount = (uint32_t)vertices.size();
}
