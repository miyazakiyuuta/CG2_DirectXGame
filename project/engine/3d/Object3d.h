#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"
#include "3d/AnimationPlayer.h"
#include "3d/Skeleton.h"

class Object3dCommon;
class Model;
class Camera;

// 3Dオブジェクト
class Object3d {
public: // メンバ関数
	void Initialize(Object3dCommon* object3dCommon);
	void Update();
	void Draw();

	Object3d();
	~Object3d();

public:
	// setter
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetColor(Vector4 color) { materialData_->color = color; }
	void SetLightColor(Vector4 color) { directionalLightData_->color = color; }
	void SetLightDirection(Vector3 direction) { directionalLightData_->direction = direction; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
	void SetEnableLighting(bool enable) { materialData_->enableLighting = enable; }
	void SetUseEnvironmentMap(bool use) { materialData_->useEnvironmentMap = use; }

	// getter
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	Vector4 GetLightColor() const { return directionalLightData_->color; }
	Vector3 GetLightDirection() const { return directionalLightData_->direction; }
	float GetLightIntensity() const { return directionalLightData_->intensity; }

private: // メンバ構造体

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		int32_t useEnvironmentMap;
		float padding[2];
		Matrix4x4 uvTransform;
		float shininess;
		float padding1[3];
	};
	
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

	void CreateMaterialData();
	void CreateTransformationMatrixData();
	void CreateDirectionalLightData();
	void DrawDebugSkeleton();

private: // メンバ変数

	Object3dCommon* object3dCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	
	Transform transform_;
	
	Camera* camera_ = nullptr;

	Model* model_ = nullptr;

	std::unique_ptr<AnimationPlayer> animationPlayer_;
	float animationTime_ = 0.0f;
	Skeleton skeleton_;
};