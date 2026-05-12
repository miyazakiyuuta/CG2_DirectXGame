#include "Object3d.h"
#include "base/SrvManager.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "debug/debugRenderer.h"

Object3d::Object3d() = default;
Object3d::~Object3d() = default;

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	object3dCommon_ = object3dCommon;
	dxCommon_ = object3dCommon_->GetDxCommon();
	
	camera_ = object3dCommon_->GetDefaultCamera();// デフォルトカメラをセットする
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	
	CreateMaterialData();
	CreateTransformationMatrixData();
	CreateDirectionalLightData();
}

void Object3d::Update() {

	Matrix4x4 worldMatrix = Matrix4x4::Affine(transform_.scale, transform_.rotate, transform_.translate);

	if (model_ && animationPlayer_) {
		animationPlayer_->Update(1.0f / 60.0f, skeleton_);
		UpdateSkeleton(skeleton_);
		model_->Update(skeleton_);
	}

    Matrix4x4 finalWorldMatrix = worldMatrix;

	transformationMatrixData_->World = finalWorldMatrix;
	transformationMatrixData_->WorldInverseTranspose = finalWorldMatrix.Inverse().Transpose();
	if (camera_) {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		transformationMatrixData_->WVP = finalWorldMatrix * viewProjectionMatrix;
	} else {
		transformationMatrixData_->WVP = finalWorldMatrix;
	}
	
}

void Object3d::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	if (model_) {
		if (!model_->GetSkinCluster().inverseBindPoseMatrices.empty()) {
			Object3dCommon::GetInstance()->ComputeDispatchSetting();
			commandList->SetComputeRootConstantBufferView(0, model_->GetSkinCluster().skinningInformationResource->GetGPUVirtualAddress());
			commandList->SetComputeRootDescriptorTable(1, model_->GetSkinCluster().paletteSrvHandle.second);
			commandList->SetComputeRootDescriptorTable(2, model_->GetSkinCluster().inputVertexSrvHandle.second);
			commandList->SetComputeRootDescriptorTable(3, model_->GetSkinCluster().influenceSrvHandle.second);
			commandList->SetComputeRootDescriptorTable(4, model_->GetSkinCluster().uavHandle.second);
			commandList->Dispatch(UINT(model_->GetModelData().vertices.size() + 1023) / 1024, 1, 1);
			Object3dCommon::GetInstance()->SkinningDrawSetting();
		} else {
			Object3dCommon::GetInstance()->CommonDrawSetting();
		}
	}

	// マテリアルCBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 座標変換行列CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	
	// 平行光源CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(4, camera_->GetGPUAddress());

	commandList->SetGraphicsRootConstantBufferView(5, object3dCommon_->GetPointLightGPUAddress());

	commandList->SetGraphicsRootConstantBufferView(6, object3dCommon_->GetSpotLightGPUAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE envHandle = object3dCommon_->GetEnvironmentSrvHandle();
	commandList->SetGraphicsRootDescriptorTable(8, envHandle);

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		if (!model_->GetSkinCluster().inverseBindPoseMatrices.empty()) {
			auto* srvManager = object3dCommon_->GetSrvManager();
			commandList->SetGraphicsRootDescriptorTable(7, srvManager->GetGPUDescriptorHandle(model_->GetSkinCluster().paletteSrvIndex));
		}
		model_->Draw();
		DrawDebugSkeleton();
	}
}

void Object3d::PlayAnimation(const std::string& name, bool loop, float blendDuration) {
	if (!model_ || !animationPlayer_) return;

	const auto& animations = model_->GetAnimations();
	auto it = animations.find(name);

	if (it != animations.end()) {
		// 再生するアニメーションを切り替える
		animationPlayer_->Play(&it->second, loop, blendDuration);
	}
}

void Object3d::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
    model_ = ModelManager::GetInstance()->FindModel(filePath);
    if (!model_) {
        // Model not found: leave model_ as nullptr to skip drawing
        return;
    }

	const std::map<std::string, Animation>& animations = model_->GetAnimations();

	if (model_ && !animations.empty()) {
		if (!animationPlayer_) {
			animationPlayer_ = std::make_unique<AnimationPlayer>();
		}
		//animationPlayer_->SetAnimation(&animations.begin()->second, true); // アニメーション開始は指定する

		// モデルのNode階層からスケルトンを生成
		skeleton_ = CreateSkeleton(model_->GetModelData().rootNode);

		model_->CreateSkinCluster(skeleton_);
	} else {
		animationPlayer_.reset();
	}
}

std::optional<Matrix4x4> Object3d::GetBoneWorldMatrix(const std::string& boneName) const {
	if (!model_) return std::nullopt;

	auto it = skeleton_.jointMap.find(boneName);
	if (it == skeleton_.jointMap.end()) return std::nullopt;

	const Joint& joint = skeleton_.joints[it->second];

	// スケルトン空間 → ワールド空間
	Matrix4x4 worldMatrix = Matrix4x4::Affine(
		transform_.scale, transform_.rotate, transform_.translate);

	return joint.skeletonSpaceMatrix * worldMatrix;
}

std::optional<Vector3> Object3d::GetBoneWorldPosition(const std::string& boneName) const {
	auto mat = GetBoneWorldMatrix(boneName);
	if (!mat) return std::nullopt;

	// 行列の平行移動成分を取り出す
	return Vector3{ mat->m[3][0], mat->m[3][1], mat->m[3][2] };
}

void Object3d::CreateMaterialData() {
	// マテリアルリソースを作る
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));

	// マテリアルリソースにデータを書き込むためのアドレスを取得してmaterialDataに割り当てる
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルデータの初期値を書き込む
	materialData_->color = Vector4{ 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = true;
	materialData_->useEnvironmentMap = false;
	materialData_->uvTransform = Matrix4x4::Identity();
	materialData_->shininess = 32.0f;
}

void Object3d::CreateTransformationMatrixData() {
	// 座標変換行列リソースを作る
	transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 単位行列に書き込んでおく
	transformationMatrixData_->WVP = Matrix4x4::Identity();
	transformationMatrixData_->World = Matrix4x4::Identity();
	transformationMatrixData_->WorldInverseTranspose = Matrix4x4::Identity();
}

void Object3d::CreateDirectionalLightData() {
	// 平行光源リソースを作る
	directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースにデータを書き込むためのアドレスを取得して平行光源構造体のポインタに割り当てる
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// デフォルト値を書き込んでおく
	directionalLightData_->color = Vector4{ 1.0f,1.0f,1.0f,1.0f };
	directionalLightData_->direction = Vector3{ 0.0f,-1.0f,0.0f };
	directionalLightData_->intensity = 1.0f; // 輝度
}

void Object3d::DrawDebugSkeleton() {
	if (!model_ || !camera_)return;

	Matrix4x4 worldMatrix = Matrix4x4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	std::vector<Vector3> jointPositions;
	for (const Joint& joint : skeleton_.joints) {
		// 骨のワールド行列
		Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * worldMatrix;

		Vector3 jointWorldPos = {
			jointWorldMatrix.m[3][0],
			jointWorldMatrix.m[3][1],
			jointWorldMatrix.m[3][2]
		};

		jointPositions.push_back(jointWorldPos);
	}
	float radius = 0.05f;
	Vector4 color = { 0.0f,0.0f,0.0f,1.0f };
	for (auto pos : jointPositions) {
		DebugRenderer::GetInstance()->AddSphere(pos, radius, color);
	}
}
