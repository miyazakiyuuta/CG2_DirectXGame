#include "effect/GPUParticleManager.h"
#include "effect/GPUParticleEmitter.h"
#include "base/SrvManager.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "utility/Logger.h"

#include <algorithm>
#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Logger;

GPUParticleManager* GPUParticleManager::instance = nullptr;

GPUParticleManager* GPUParticleManager::GetInstance() {
	if (!instance) instance = new GPUParticleManager();
	return instance;
}

void GPUParticleManager::Finalize() {
	delete instance;
	instance = nullptr;
}

void GPUParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	assert(dxCommon_);
	srvManager_ = srvManager;
	assert(srvManager_);

	CreateSharedBuffers();
	CreateInitCSPipeline();
	CreateEmitCSPipeline();
	CreateUpdateCSPipeline();
	CreateDrawPipeline();
}

void GPUParticleManager::Update(float deltaTime) {
	perFrameData_->time += deltaTime;
	perFrameData_->deltaTime = deltaTime;

	// エミッタの現在値をグループのCBVへ書き込み、射出タイミングをCPU側で判定する
	// (発生処理そのものはEmit CSが行う。1グループにつきエミッタは1つを想定)
	for (GPUParticleEmitter* emitter : emitters_) {
		auto it = particleGroups_.find(emitter->GetGroupName());
		if (it == particleGroups_.end()) { continue; }
		ParticleGroup& group = it->second;

		EmitterSphere* emitterData = group.emitterData;
		emitterData->translate = emitter->GetPosition();
		emitterData->radius = emitter->GetRadius();
		emitterData->count = emitter->GetEmitCount();
		emitterData->frequency = emitter->GetFrequency();

		if (emitter->IsActive() && emitterData->frequency > 0.0f) {
			emitterData->frequencyTime += deltaTime;
			// 射出間隔を上回ったら射出許可を出して時間を調整
			if (emitterData->frequency <= emitterData->frequencyTime) {
				emitterData->frequencyTime -= emitterData->frequency;
				emitterData->emit = 1;
			} else {
				emitterData->emit = 0;
			}
		} else {
			emitterData->emit = 0;
		}

		// ImGuiでのライブ編集を即反映するため毎フレーム書き込む(Mapしっぱなしなので軽い)
		WriteConfig(group, emitter->GetConfig());
	}
}

void GPUParticleManager::DispatchAll() {
	if (particleGroups_.empty()) { return; }

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	assert(commandList);

	for (auto& [name, group] : particleGroups_) {
		// バッファはExecuteCommandLists完了時にCOMMONへ戻り、最初のDispatchアクセスでUAVへ暗黙昇格する。
		// そのためフレーム先頭でのUAVへの遷移バリアは不要(スキニングと同じ考え方)

		// 生成直後のグループは全スロットを死亡状態にしてフリーリストを構築する
		if (group.needsInitialize) {
			commandList->SetComputeRootSignature(rootSignatureInitCS_.Get());
			commandList->SetPipelineState(pipelineStateInitCS_.Get());
			commandList->SetComputeRootDescriptorTable(
				0, srvManager_->GetGPUDescriptorHandle(group.particleUavIndex));
			commandList->Dispatch(kMaxParticles / kThreadGroupSize, 1, 1);
			BarrierUAV(commandList, group);
			group.needsInitialize = false;
		}

		// 射出許可が出たフレームだけEmit CSを流す(1スレッド=1粒)
		if (group.emitterData->emit != 0) {
			commandList->SetComputeRootSignature(rootSignatureEmitCS_.Get());
			commandList->SetPipelineState(pipelineStateEmitCS_.Get());
			commandList->SetComputeRootDescriptorTable(
				0, srvManager_->GetGPUDescriptorHandle(group.particleUavIndex));
			commandList->SetComputeRootConstantBufferView(1, group.emitterResource->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(2, perFrameResource_->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(3, group.configResource->GetGPUVirtualAddress());
			commandList->Dispatch((group.emitterData->count + kThreadGroupSize - 1) / kThreadGroupSize, 1, 1);
			BarrierUAV(commandList, group);
		}

		// 全粒子の更新(移動・寿命・色/スケール補間・死亡時のフリーリスト返却)
		commandList->SetComputeRootSignature(rootSignatureUpdateCS_.Get());
		commandList->SetPipelineState(pipelineStateUpdateCS_.Get());
		commandList->SetComputeRootDescriptorTable(
			0, srvManager_->GetGPUDescriptorHandle(group.particleUavIndex));
		commandList->SetComputeRootConstantBufferView(1, perFrameResource_->GetGPUVirtualAddress());
		commandList->SetComputeRootConstantBufferView(2, group.configResource->GetGPUVirtualAddress());
		commandList->Dispatch(kMaxParticles / kThreadGroupSize, 1, 1);

		// CSの書き込み完了を待ちつつ、粒子バッファをVSから読める状態へ遷移する
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = group.particleResource.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &barrier);
	}
}

void GPUParticleManager::Draw() {
	if (!camera_ || particleGroups_.empty()) { return; }

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	assert(commandList);

	perViewData_->viewProjection = camera_->GetViewProjectionMatrix();
	perViewData_->billboardMatrix = camera_->GetBillboardMatrix();

	commandList->SetGraphicsRootSignature(rootSignatureDraw_.Get());
	commandList->SetPipelineState(pipelineStateDraw_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, materialResource_->GetGPUVirtualAddress());

	for (auto& [name, group] : particleGroups_) {
		if (group.needsInitialize) { continue; } // まだInitialize CSが流れていない

		commandList->SetGraphicsRootDescriptorTable(
			1, srvManager_->GetGPUDescriptorHandle(group.particleSrvIndex));
		commandList->SetGraphicsRootDescriptorTable(
			3, srvManager_->GetGPUDescriptorHandle(group.textureSrvIndex));

		// 死亡スロットはscale=0の縮退三角形になりピクセルを出さないため、常に最大数で描いてよい
		// (生存数だけ描くにはExecuteIndirectが必要。将来の発展課題)
		commandList->DrawInstanced(6, kMaxParticles, 0, 0);
	}
}

void GPUParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath, const GPUParticleConfig& config) {
	if (particleGroups_.contains(name)) { return; } // 登録済みなら何もしない(冪等)

	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	ParticleGroup group{};
	group.textureFilePath = textureFilePath;
	group.textureSrvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

	// 粒子・フリーリストのUAVバッファ
	group.particleResource = CreateUavBuffer(sizeof(ParticleCS) * kMaxParticles);
	group.freeListIndexResource = CreateUavBuffer(sizeof(int32_t));
	group.freeListResource = CreateUavBuffer(sizeof(uint32_t) * kMaxParticles);

	// UAVは1つのDescriptorTableでu0〜u2に渡すため、必ずこの順で連続確保する
	group.particleUavIndex = srvManager_->Allocate();
	srvManager_->CreateUAVForStructuredBuffer(group.particleUavIndex, group.particleResource.Get(), kMaxParticles, sizeof(ParticleCS));
	uint32_t freeListIndexUav = srvManager_->Allocate();
	srvManager_->CreateUAVForStructuredBuffer(freeListIndexUav, group.freeListIndexResource.Get(), 1, sizeof(int32_t));
	uint32_t freeListUav = srvManager_->Allocate();
	srvManager_->CreateUAVForStructuredBuffer(freeListUav, group.freeListResource.Get(), kMaxParticles, sizeof(uint32_t));
	assert(freeListIndexUav == group.particleUavIndex + 1 && freeListUav == group.particleUavIndex + 2);

	group.particleSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVForStructuredBuffer(group.particleSrvIndex, group.particleResource.Get(), kMaxParticles, sizeof(ParticleCS));

	// エミッタCBV(値は毎フレームUpdateでエミッタから書き込まれる)
	group.emitterResource = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));
	group.emitterResource->Map(0, nullptr, reinterpret_cast<void**>(&group.emitterData));
	*group.emitterData = {};

	// configCBV
	group.configResource = dxCommon_->CreateBufferResource(sizeof(GPUParticleConfigForGPU));
	group.configResource->Map(0, nullptr, reinterpret_cast<void**>(&group.configData));
	WriteConfig(group, config);

	particleGroups_.emplace(name, std::move(group));
}

void GPUParticleManager::RegisterEmitter(GPUParticleEmitter* emitter) {
	emitters_.push_back(emitter);
}

void GPUParticleManager::UnregisterEmitter(GPUParticleEmitter* emitter) {
	emitters_.erase(std::remove(emitters_.begin(), emitters_.end(), emitter), emitters_.end());
}

void GPUParticleManager::DrawImGui() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("GPU Groups")) {
		for (auto& [name, group] : particleGroups_) {
			ImGui::PushID(name.c_str());
			if (ImGui::TreeNode(name.c_str())) {
				// 生存数はGPU側(フリーリスト)にしかないためCPUからは表示できない
				ImGui::Text("capacity: %u", kMaxParticles);
				ImGui::Text("texture: %s", group.textureFilePath.c_str());
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
	if (ImGui::CollapsingHeader("GPU Emitters")) {
		int id = 0;
		for (GPUParticleEmitter* emitter : emitters_) {
			ImGui::PushID(id++);
			if (ImGui::TreeNode(emitter->GetGroupName().c_str())) {
				emitter->DrawImGui();
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
#endif
}

void GPUParticleManager::DrawConfigImGui(GPUParticleConfig& config) {
#ifdef USE_IMGUI
	ImGui::DragFloat3("minScale", &config.minScale.x, 0.01f);
	ImGui::DragFloat3("maxScale", &config.maxScale.x, 0.01f);
	ImGui::DragFloat3("minVelocity", &config.minVelocity.x, 0.01f);
	ImGui::DragFloat3("maxVelocity", &config.maxVelocity.x, 0.01f);
	ImGui::DragFloat3("acceleration", &config.acceleration.x, 0.01f);
	ImGui::DragFloat("lifeTimeMin", &config.lifeTimeMin, 0.01f, 0.0f, 60.0f);
	ImGui::DragFloat("lifeTimeMax", &config.lifeTimeMax, 0.01f, 0.0f, 60.0f);
	ImGui::ColorEdit4("startColor", &config.startColor.x);
	ImGui::ColorEdit4("endColor", &config.endColor.x);
	ImGui::DragFloat("endScaleRatio", &config.endScaleRatio, 0.01f, 0.0f, 10.0f);
#else
	(void)config;
#endif
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUParticleManager::CreateUavBuffer(uint64_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = sizeInBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COMMON, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	(void)hr;
	return resource;
}

void GPUParticleManager::BarrierUAV(ID3D12GraphicsCommandList* commandList, ParticleGroup& group) {
	D3D12_RESOURCE_BARRIER barriers[3]{};
	ID3D12Resource* resources[3] = {
		group.particleResource.Get(),
		group.freeListIndexResource.Get(),
		group.freeListResource.Get(),
	};
	for (int i = 0; i < 3; ++i) {
		barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[i].UAV.pResource = resources[i];
	}
	commandList->ResourceBarrier(_countof(barriers), barriers);
}

void GPUParticleManager::WriteConfig(ParticleGroup& group, const GPUParticleConfig& config) {
	GPUParticleConfigForGPU* data = group.configData;
	data->minScale = config.minScale;
	data->maxScale = config.maxScale;
	data->minVelocity = config.minVelocity;
	data->maxVelocity = config.maxVelocity;
	data->acceleration = config.acceleration;
	data->startColor = config.startColor;
	data->endColor = config.endColor;
	data->lifeTimeMin = config.lifeTimeMin;
	data->lifeTimeMax = config.lifeTimeMax;
	data->endScaleRatio = config.endScaleRatio;
}

void GPUParticleManager::CreateSharedBuffers() {
	perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerView));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	*perViewData_ = {};

	perFrameResource_ = dxCommon_->CreateBufferResource(sizeof(PerFrame));
	perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
	*perFrameData_ = {};

	materialResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialForGPU));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = 0;
	materialData_->uvTransform = Matrix4x4::Identity();
}

void GPUParticleManager::CreateInitCSPipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	// u0:粒子 u1:freeListIndex u2:freeList(連続確保したUAVを1テーブルで渡す)
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 3;
	uavRange.BaseShaderRegister = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[1]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureInitCS_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(
		L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureInitCS_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateInitCS_));
	assert(SUCCEEDED(hr));
}

void GPUParticleManager::CreateEmitCSPipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 3;
	uavRange.BaseShaderRegister = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4]{};
	// [0] u0〜u2: UAV DescriptorTable
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	// [1] b0: EmitterSphere
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	// [2] b1: PerFrame
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1;
	// [3] b2: ParticleConfig
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 2;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureEmitCS_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(
		L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureEmitCS_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateEmitCS_));
	assert(SUCCEEDED(hr));
}

void GPUParticleManager::CreateUpdateCSPipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 3;
	uavRange.BaseShaderRegister = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};
	// [0] u0〜u2: UAV DescriptorTable
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	// [1] b1: PerFrame(Emit CSとレジスタ番号を揃えている)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	// [2] b2: ParticleConfig
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 2;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureUpdateCS_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(
		L"resources/shaders/UpdateParticle.CS.hlsl", L"cs_6_0");
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureUpdateCS_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateUpdateCS_));
	assert(SUCCEEDED(hr));
}

void GPUParticleManager::CreateDrawPipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	// t0: 粒子SRV(VS用)
	D3D12_DESCRIPTOR_RANGE particleRange{};
	particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleRange.NumDescriptors = 1;
	particleRange.BaseShaderRegister = 0;
	particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t0: テクスチャ(PS用)
	D3D12_DESCRIPTOR_RANGE texRange{};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.NumDescriptors = 1;
	texRange.BaseShaderRegister = 0;
	texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4]{};
	// [0] b0: PerView(VS用)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	// [1] t0: 粒子SRV(VS用)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &particleRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	// [2] b0: Material(PS用)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 0;
	// [3] t0: テクスチャ(PS用)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &texRange;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 頂点バッファを使わない(SV_VertexIDで四角形を作る)ためIA入力レイアウトは不要
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatureDraw_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(
		L"resources/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
	assert(vsBlob != nullptr);
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(
		L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(psBlob != nullptr);

	// BlendState(加算合成。順序非依存なのでソート不要)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// 半透明なので深度は読むが書かない
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureDraw_.Get();
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateDraw_));
	assert(SUCCEEDED(hr));
}
