#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"


using namespace MatrixMath;

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	object3dCommon_ = object3dCommon;
	dxCommon_ = object3dCommon_->GetDxCommon();

	// Transform変数を作る
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	cameraTransform_ = { {1.0f,1.0f,1.0f},{0.3f,0.0f,0.0f},{0.0f,4.0f,-10.0f} };
	
	CreateTransformationMatrixData();
	CreateDirectionalLightData();
}

void Object3d::Update() {
	// TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	// cameraTransformからcameraMatrixを作る
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	// cameraMatrixからViewMatrixを作る
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	// ProjectionMatrixを作って透視投影行列を書き込む
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);

	transformationMatrixData_->WVP = Multiply(Multiply(worldMatrix, viewMatrix), projectionMatrix);
	transformationMatrixData_->World = worldMatrix;
}

void Object3d::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// 座標変換行列CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	
	// 平行光源CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
	
	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}
}

void Object3d::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	assert(model_ && "Model not found. filePath key mismatch.");
}

void Object3d::CreateTransformationMatrixData() {
	// 座標変換行列リソースを作る
	transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 単位行列に書き込んでおく
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLightData() {
	// 平行光源リソースを作る
	directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースにデータを書き込むためのアドレスを取得して平行光源構造体のポインタに割り当てる
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// デフォルト値を書き込んでおく
	directionalLightData_->color = Vector4{ 1.0f,1.0f,1.0f,1.0f };
	directionalLightData_->direction = Vector3{ 0.0f,-1.0f,0.0f };
	directionalLightData_->intensity = 1.0f;
}