#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "debug/debugSphere.h"
#include "utility/Logger.h"

Object3d::Object3d() = default;
Object3d::~Object3d() = default;

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	object3dCommon_ = object3dCommon;
	dxCommon_ = object3dCommon_->GetDxCommon();
	// デフォルトカメラをセットする
	camera_ = object3dCommon_->GetDefaultCamera();

	// Transform変数を作る
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	
	CreateTransformationMatrixData();
	CreateDirectionalLightData();
}

void Object3d::Update() {

	Matrix4x4 worldMatrix = Matrix4x4::Affine(transform_.scale, transform_.rotate, transform_.translate);

	if (model_ && !model_->GetAnimation().nodeAnimations.empty()) {
		float deltaTime = 1.0f / 60.0f; // 本来は受け取る
		animationTime_ += 1.0f / 60.0f;
		animationTime_ = std::fmod(animationTime_, model_->GetAnimation().duration);
		ApplyAnimation(skeleton_, model_->GetAnimation(), animationTime_);
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
			Object3dCommon::GetInstance()->SkinningDrawSetting();
		} else {
			Object3dCommon::GetInstance()->CommonDrawSetting();
		}
	}

	// 座標変換行列CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	
	// 平行光源CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(4, camera_->GetGPUAddress());

	commandList->SetGraphicsRootConstantBufferView(5, object3dCommon_->GetPointLightGPUAddress());

	commandList->SetGraphicsRootConstantBufferView(6, object3dCommon_->GetSpotLightGPUAddress());

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
		//DrawDebugSkeleton();
	}
}

void Object3d::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	assert(model_ && "Model not found. filePath key mismatch.");

	if (model_ && !model_->GetAnimation().nodeAnimations.empty()) {
		if (!animationPlayer_) {
			animationPlayer_ = std::make_unique<AnimationPlayer>();
		}
		animationPlayer_->SetAnimation(&model_->GetAnimation());

		// モデルのNode階層からスケルトンを生成
		skeleton_ = CreateSkeleton(model_->GetModelData().rootNode);

		model_->CreateSkinCluster(skeleton_);
	} else {
		animationPlayer_.reset();
	}
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
	directionalLightData_->intensity = 0.0f;
	//directionalLightData_->intensity = 1.0f;
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
	//debugSphere_->Draw(jointPositions, radius, color, *camera_);
}
