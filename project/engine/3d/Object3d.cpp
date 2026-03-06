#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	object3dCommon_ = object3dCommon;
	dxCommon_ = object3dCommon_->GetDxCommon();
	// デフォルトカメラをセットする
	camera_ = object3dCommon_->GetDefaultCamera();

	// Transform変数を作る
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	
	CreateTransformationMatrixData();
	CreateDirectionalLightData();

	animationPlayer_ = new AnimationPlayer(); // コンストラクタを切り替Matrix4x4::えられるようにする & unique_ptrにする
}

void Object3d::Update() {

	Matrix4x4 worldMatrix = Matrix4x4::Affine(transform_.scale, transform_.rotate, transform_.translate);

	if (animationPlayer_ && model_ && model_->GetAnimation().duration > 0.0f) {
		animationPlayer_->Update(1.0f / 60.0f, model_->GetModelData().rootNode.name);
		worldMatrix = animationPlayer_->GetLocalMatrix() * worldMatrix;
	}

	Matrix4x4 finalWorldMatrix = model_->GetModelData().rootNode.localMatrix * worldMatrix;

	transformationMatrixData_->World = finalWorldMatrix;
	transformationMatrixData_->WorldInverseTranspose = finalWorldMatrix.Inverse().Transpose();
	if (camera_) {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		transformationMatrixData_->WVP = finalWorldMatrix * viewProjectionMatrix;
	} else {
		transformationMatrixData_->WVP = finalWorldMatrix;
	}
	
	/*
	Matrix4x4 worldViewProjectionMatrix;
	if (camera_) {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		worldViewProjectionMatrix = worldMatrix;
	}

	transformationMatrixData_->WVP = Multiply(model_->GetModelData().rootNode.localMatrix, worldViewProjectionMatrix);
	transformationMatrixData_->World = Multiply(model_->GetModelData().rootNode.localMatrix, worldMatrix);
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
	*/
}

void Object3d::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

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
	}
}

void Object3d::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	assert(model_ && "Model not found. filePath key mismatch.");

	if (animationPlayer_) {
		animationPlayer_->SetAnimation(&model_->GetAnimation());
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
}