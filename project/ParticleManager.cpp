#include "ParticleManager.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Camera.h"
#include "Logger.h"

#include <numbers>

using namespace MatrixMath;
using namespace Logger;

ParticleManager* ParticleManager::instance = nullptr;

ParticleManager* ParticleManager::GetInstance() {
	if (!instance)instance = new ParticleManager();
	return instance;
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	assert(dxCommon_);
	srvManager_ = srvManager;
	assert(srvManager_);

	// MaterialCB作成
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialForGPU));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = 0;
	materialData_->uvTransform = MakeIdentity4x4();

	CreateVertexResource();
	CreateGraphicsPipelineState();
}

void ParticleManager::Update(float deltaTime) {
	assert(camera_);
	const Matrix4x4& view = camera_->GetViewMatrix();
	const Matrix4x4& proj = camera_->GetProjectionMatrix();
	Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();

	// instancing 用のWVP行列を作る
	
	Matrix4x4 viewProjectionMatrix = Multiply(view, proj);

	for (auto& [name, group] : particleGroups_) {
		uint32_t numInstance = 0;
		for (std::list<Particle>::iterator particleIterator = group.particles.begin();
			particleIterator != group.particles.end();) {
			particleIterator->currentTime += deltaTime;
			if ((*particleIterator).lifeTime <= (*particleIterator).currentTime) {
				particleIterator = group.particles.erase(particleIterator); // 生存期間が過ぎたParticleはlistから消す。戻り値が次のイテレータとなる。
				continue;
			}

			if (numInstance < kNumMaxInstance) {
				particleIterator->transform.translate.x += particleIterator->velocity.x * deltaTime;
				particleIterator->transform.translate.y += particleIterator->velocity.y * deltaTime;
				particleIterator->transform.translate.z += particleIterator->velocity.z * deltaTime;

				Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
				Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, cameraMatrix);
				billboardMatrix.m[3][0] = 0.0f; // 平行移動成分はいらない
				billboardMatrix.m[3][1] = 0.0f;
				billboardMatrix.m[3][2] = 0.0f;
				Matrix4x4 rotateZMatrix = MakeRotateZMatrix(particleIterator->transform.rotate.z + 0.0f);
				Matrix4x4 worldMatrix = Multiply(
					Multiply(Multiply(MakeScaleMatrix(particleIterator->transform.scale), rotateZMatrix), billboardMatrix),
					MakeTranslateMatrix(particleIterator->transform.translate));
				Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
				
				group.instancingData[numInstance].wvp = worldViewProjectionMatrix;
				group.instancingData[numInstance].world = worldMatrix;
				group.instancingData[numInstance].color = particleIterator->color;
				float alpha = 1.0f - (particleIterator->currentTime / particleIterator->lifeTime);
				group.instancingData[numInstance].color.w = alpha;
				++numInstance;
			}
			++particleIterator; // 次のイテレータに進める
		}
		group.instanceCount = numInstance;
	}
}

void ParticleManager::Draw() {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	assert(commandList);

	// ルートシグネチャをセットするコマンド
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	// グラフィックスパイプラインステートをセットするコマンド
	commandList->SetPipelineState(pipelineState_.Get());
	// プリミティブトポロジーをセットするコマンド
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// Vertex Buffer Viewをセットするコマンド
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	//commandList->SetGraphicsRootConstantBufferView(0, camera_->GetGPUAddress());
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	for(auto& [_, group]:particleGroups_) {
		if (group.instanceCount == 0) { continue; }
		
		D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(group.instancingSrvIndex);
		commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(group.material.srvIndex);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
		commandList->DrawInstanced(6, group.instanceCount, 0, 0);
	}
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath) {
	assert(particleGroups_.find(name) == particleGroups_.end() && "ParticleGroup already exists");

	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	ParticleGroup group{};
	group.material.textureFilePath = textureFilePath;
	group.material.srvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

	CreateInstancingResource(group);

	particleGroups_.emplace(name, std::move(group));
}

void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count) {
	auto itGroup = particleGroups_.find(name);
	assert(itGroup != particleGroups_.end() && "ParticleGroup not found. Call CreateParticleGroup first.");

	ParticleGroup& group = itGroup->second;

	for (uint32_t i = 0; i < count; ++i) {
		std::uniform_real_distribution<float> distribution(-5.0f, 5.0f);
		Particle p{};

		p.transform.translate = position;
		p.transform.scale = { 1.0f,1.0f,1.0f };
		p.transform.rotate = { 0.0f,0.0f,0.0f };
		p.velocity.y = distribution(randomEngine);
		p.color = { 1.0f,1.0f,1.0f,1.0f };
		p.lifeTime = 0.5f;
		p.currentTime = 0.0f;

		group.particles.push_back(p);
	}
}

void ParticleManager::CreateInstancingResource(ParticleGroup& group) {
	group.instanceCount = 0;
	
	const UINT bufferSize = sizeof(InstanceData) * kNumMaxInstance;

	group.instancingResource = dxCommon_->CreateBufferResource(bufferSize);
	group.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instancingData));
	group.instancingSrvIndex = srvManager_->Allocate();

	srvManager_->CreateSRVForStructuredBuffer(group.instancingSrvIndex, group.instancingResource.Get(), kNumMaxInstance, sizeof(InstanceData));
}

void ParticleManager::CreateRootSignature() {
	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE rangeForInstancing[1] = {};
	rangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	rangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	rangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	rangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
	srvRange[0].BaseShaderRegister = 0; // 0から始まる
	srvRange[0].NumDescriptors = 1; // 数は1つ
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	//rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = rangeForInstancing; // Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(rangeForInstancing); // Tableで利用する数

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = srvRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(srvRange); // Tableで利用する数

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

	rootSignatureDesc.pStaticSamplers = staticSamplers;
	rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	rootSignatureDesc.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
	rootSignatureDesc.NumParameters = _countof(rootParameters); // 配列の長さ

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	ID3D12Device* device = dxCommon_->GetDevice();
	
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGraphicsPipelineState() {
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//int particleBlendMode = kBlendModeAdd;
	//int particleBlendMode = kBlendModeNormal;
	//int particlePrevBlendMode = -1;
	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// ブレンドモード
	/* ノーマル
	particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	*/
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	// RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//D3D12_RASTERIZER_DESC rasterizerDesc = D3D12_RASTERIZER_DESC(D3D12_DEFAULT);
	// 裏面(時計回り)を表示しない
	//rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = rootSignature_.Get(); // RootSignature
	pipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() }; // VertexShader
	pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() }; // PixelShader
	pipelineStateDesc.BlendState = blendDesc; // BlendState
	pipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ。三角形
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	//depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Depthの書き込みを行わない
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// DepthStencilの設定
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	ID3D12Device* device = dxCommon_->GetDevice();
	HRESULT hr = device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

	/*
	
	if (particleBlendMode != particlePrevBlendMode) {
		particlePrevBlendMode = particleBlendMode;
		if (particleBlendMode == kBlendModeNone) {
			particleBlendDesc.RenderTarget[0].BlendEnable = FALSE;
		} else {
			particleBlendDesc.RenderTarget[0].BlendEnable = TRUE;
			particleBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			particleBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			particleBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

			if (particleBlendMode == kBlendModeNormal) {
				particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			}
			if (particleBlendMode == kBlendModeAdd) {
				particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			}
			if (particleBlendMode == kBlendModeSubtract) {
				particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
				particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			}
			if (particleBlendMode == kBlendModeMultiply) {
				particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
				particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
			}
			if (particleBlendMode == kBlendModeScreen) {
				particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
				particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			}
			particlePipelineStateDesc.BlendState = particleBlendDesc;
			hr = device->CreateGraphicsPipelineState(
				&particlePipelineStateDesc, IID_PPV_ARGS(&particlePipelineState));
			assert(SUCCEEDED(hr));
		}
	}
	*/

}

void ParticleManager::CreateVertexResource() {
	constexpr uint32_t kVertexCount = 6;
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kVertexCount);

	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点のサイズ
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * kVertexCount);
	// 1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	static const VertexData kQuadVertices[6] = {
		// tri1
		{{-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0,0,1}},
		{{ 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0,0,1}},
		{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0,0,1}},
		// tri2
		{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0,0,1}},
		{{ 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0,0,1}},
		{{ 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0,0,1}},
	};

	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, kQuadVertices, sizeof(VertexData) * kVertexCount);
	vertexResource_->Unmap(0, nullptr);
}
