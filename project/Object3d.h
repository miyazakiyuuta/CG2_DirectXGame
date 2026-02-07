#pragma once
#include "DirectXCommon.h"
#include "engine/base/Matrix4x4.h"
#include "engine/base/Vector3.h"
#include "engine/base/Vector4.h"
#include "Transform.h"

class Object3dCommon;
class Model;
class Camera;

// 3Dオブジェクト
class Object3d {
public: // メンバ関数
	void Initialize(Object3dCommon* object3dCommon);
	void Update();
	void Draw();

public:
	// setter
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetLightColor(Vector4 color) { directionalLightData_->color = color; }
	void SetLightDirection(Vector3 direction) { directionalLightData_->direction = direction; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

	// getter
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	//DirectionalLight* GetDirectionalLightData() const { return directionalLightData_; }
	Vector4 GetLightColor() const { return directionalLightData_->color; }
	Vector3 GetLightDirection() const { return directionalLightData_->direction; }
	float GetLightIntensity() const { return directionalLightData_->intensity; }

private: // メンバ構造体
	
	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	struct DirectionalLight {
		Vector4 color; //!< ライトの色
		Vector3 direction; //!< ライトの向き
		float intensity; //!< 輝度
	};

private: // メンバ関数

	void CreateTransformationMatrixData();
	void CreateDirectionalLightData();

private: // メンバ変数

	Object3dCommon* object3dCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;
	
	Transform transform_;
	
	Camera* camera_ = nullptr;

	Model* model_ = nullptr;
};


