#include "GPUParticleEmitter.h"

void GPUParticleEmitter::Initialize() {
	CreateResource();
	CreateCSPipelineState();
	CreateEmitCSPipelineState();
	CreateDrawPipelineState();
	CreatePerViewBuffer();
	CreateMaterialBuffer();
	CreateEmitterBuffer();
	CreatePerFrameBuffer();
	InitializeParticles();
}

void GPUParticleEmitter::Update(float deltaTime) {
	perFrameData_->time += deltaTime;
	emitterSphere_->frequencyTime += deltaTime;
	//射出間隔を上回ったら射出許可を出して時間を調整
	if (emitterSphere_->frequency <= emitterSphere_->frequencyTime) {
		emitterSphere_->frequencyTime -= emitterSphere_->frequency;
		emitterSphere_->emit = 1;
	} else { // 射出間隔を上回っていないので、射出許可は出せない
		emitterSphere_->emit = 0;
	}
}

void GPUParticleEmitter::Draw(Camera* camera) {
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	EmitParticles();

	// PerViewを更新
	perViewData_->viewProjection = camera->GetViewProjectionMatrix();
	perViewData_->billboardMatrix = camera->GetBillboardMatrix();

	// 描画用RootSignature・PSOをセット
	commandList->SetGraphicsRootSignature(rootSignatureDraw_.Get());
	commandList->SetPipelineState(pipelineStateDraw_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// b0: PerView
	commandList->SetGraphicsRootConstantBufferView(
		0, perViewResource_->GetGPUVirtualAddress());

	// t0: Particle SRV
	commandList->SetGraphicsRootDescriptorTable(
		1, SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_));

	commandList->SetGraphicsRootConstantBufferView(2, materialResource_->GetGPUVirtualAddress());

	commandList->SetGraphicsRootDescriptorTable(
		3, SrvManager::GetInstance()->GetGPUDescriptorHandle(textureSrvIndex_));

	// DrawInstanced(頂点6枚, 1024個, 0, 0)
	commandList->DrawInstanced(6, kMaxParticles, 0, 0);
}

void GPUParticleEmitter::CreateResource() {
	auto device = DirectXCommon::GetInstance()->GetDevice();

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = sizeof(ParticleCS) * kMaxParticles;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAV用

	device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COMMON, nullptr,
		IID_PPV_ARGS(&particleResource_)
	);

	// SrvManagerのUAV確保
	uavIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(uavIndex_, particleResource_.Get(), kMaxParticles, sizeof(ParticleCS));

	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC counterDesc{};
	counterDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterDesc.Width = sizeof(int32_t);
	counterDesc.Height = 1;
	counterDesc.DepthOrArraySize = 1;
	counterDesc.MipLevels = 1;
	counterDesc.SampleDesc.Count = 1;
	counterDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &counterDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr,
		IID_PPV_ARGS(&freeCounterResource_));

	freeCounterUavIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		freeCounterUavIndex_, freeCounterResource_.Get(), 1, sizeof(int32_t));

	srvIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateSRVForStructuredBuffer(srvIndex_, particleResource_.Get(), kMaxParticles, sizeof(ParticleCS));
}

void GPUParticleEmitter::CreateCSPipelineState() {
	auto device = DirectXCommon::GetInstance()->GetDevice();

	// UAV用のDescriptorRange
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 2;
	uavRange.BaseShaderRegister = 0; // u0
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter
	D3D12_ROOT_PARAMETER rootParameters[1]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// RootSignature
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureCS_));
	assert(SUCCEEDED(hr));

	// CS用PSO
	Microsoft::WRL::ComPtr<IDxcBlob> csBlob = DirectXCommon::GetInstance()->CompileShader(
		L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureCS_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateCS_));
	assert(SUCCEEDED(hr));

}

void GPUParticleEmitter::CreateEmitCSPipelineState() {
	auto device = DirectXCommon::GetInstance()->GetDevice();

	// [0] u0: UAV（gParticles）
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 2;
	uavRange.BaseShaderRegister = 0; // u0
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};

	// [0] u0: UAV DescriptorTable
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// [1] b0: EmitterSphere CBV
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0

	// [2] b1: PerFrame CBV
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1; // b1

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = device->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatureEmitCS_));
	assert(SUCCEEDED(hr));

	// EmitParticle CS のコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> csBlob =
		DirectXCommon::GetInstance()->CompileShader(
			L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureEmitCS_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	hr = device->CreateComputePipelineState(
		&psoDesc, IID_PPV_ARGS(&pipelineStateEmitCS_));
	assert(SUCCEEDED(hr));
}

void GPUParticleEmitter::CreateDrawPipelineState() {
	auto device = DirectXCommon::GetInstance()->GetDevice();

	// t0: StructuredBuffer<Particle>（VS用）
	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0; // t0
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t1: テクスチャ（PS用）
	D3D12_DESCRIPTOR_RANGE texRange{};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.NumDescriptors = 1;
	texRange.BaseShaderRegister = 0; // t0
	texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter
	D3D12_ROOT_PARAMETER rootParameters[4]{};
	// [0] b0: PerView（VS用）
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0; // b0

	// [1] t0: Particle SRV（VS用）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2] b0: Material CBV（PS用）
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 0;


	// [3] t1: テクスチャ（PS用）
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &texRange;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	// Sampler
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// RootSignature
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureDraw_));
	assert(SUCCEEDED(hr));

	// Shaderコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = DirectXCommon::GetInstance()->CompileShader(
		L"resources/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
	assert(vsBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = DirectXCommon::GetInstance()->CompileShader(
		L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(psBlob != nullptr);

	// InputLayout（position, texcoord, normalのみ）
	D3D12_INPUT_ELEMENT_DESC inputElements[3]{};
	inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT };
	inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT };
	inputElements[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT };

	//D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };
	D3D12_INPUT_LAYOUT_DESC inputLayout{};
	inputLayout.pInputElementDescs = nullptr;
	inputLayout.NumElements = 0;

	// BlendState（アルファブレンド）
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState（カリングなし）
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// DepthStencil（書き込みなし）
	D3D12_DEPTH_STENCIL_DESC depthDesc{};
	depthDesc.DepthEnable = true;
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 半透明なので書き込まない
	depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureDraw_.Get();
	psoDesc.InputLayout = inputLayout;
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateDraw_));
	assert(SUCCEEDED(hr));
}

void GPUParticleEmitter::CreatePerViewBuffer() {
	perViewResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(PerView));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	*perViewData_ = {};
}

void GPUParticleEmitter::CreateMaterialBuffer() {
	materialResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 0;
	materialData_->uvTransform = Matrix4x4::Identity();
}

void GPUParticleEmitter::CreateEmitterBuffer() {
	emitterResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(EmitterSphere));
	emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterSphere_));

	emitterSphere_->count = 10;
	emitterSphere_->frequency = 0.5f;
	emitterSphere_->frequencyTime = 0.0f;
	emitterSphere_->translate = Vector3(0.0f, 0.0f, 0.0f);
	emitterSphere_->radius = 1.0f;
	emitterSphere_->emit = 0;
}

void GPUParticleEmitter::CreatePerFrameBuffer() {
	perFrameResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(PerFrame));
	perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
	perFrameData_->time = 0.0f;
}

void GPUParticleEmitter::InitializeParticles() {
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	SrvManager::GetInstance()->PreDraw();

	// CS用のRootSignature・PSOをセット
	commandList->SetComputeRootSignature(rootSignatureCS_.Get());
	commandList->SetPipelineState(pipelineStateCS_.Get());

	// UAVをセット（u0）
	commandList->SetComputeRootDescriptorTable(
		0, SrvManager::GetInstance()->GetGPUDescriptorHandle(uavIndex_));

	// Dispatch(1, 1, 1)　※1024スレッドx1x1
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = particleResource_.Get();
	commandList->ResourceBarrier(1, &barrier);
}

void GPUParticleEmitter::EmitParticles() {
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	commandList->SetComputeRootSignature(rootSignatureEmitCS_.Get());
	commandList->SetPipelineState(pipelineStateEmitCS_.Get());

	// [0] u0: UAV
	commandList->SetComputeRootDescriptorTable(
		0, SrvManager::GetInstance()->GetGPUDescriptorHandle(uavIndex_));

	// [1] b0: EmitterSphere CBV
	commandList->SetComputeRootConstantBufferView(
		1, emitterResource_->GetGPUVirtualAddress());

	commandList->SetComputeRootConstantBufferView(
		2, perFrameResource_->GetGPUVirtualAddress()); // b1

	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = particleResource_.Get();
	commandList->ResourceBarrier(1, &barrier);
}
