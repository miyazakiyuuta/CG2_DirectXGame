#pragma once
#include "DirectXCommon.h"
#include "engine/base/Matrix4x4.h"
#include "engine/base/Vector3.h"

class Object3dCommon;
class Model;

// 3Dオブジェクト
class Object3d {
public: // メンバ関数
	void Initialize(Object3dCommon* object3dCommon);
	void Update();
	void Draw();

public:
	// setter
	void SetModel(const std::string& filePath);
	void SetScale(const MatrixMath::Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const MatrixMath::Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const MatrixMath::Vector3& translate) { transform_.translate = translate; }

	// getter
	const MatrixMath::Vector3& GetScale() const { return transform_.scale; }
	const MatrixMath::Vector3& GetRotate() const { return transform_.rotate; }
	const MatrixMath::Vector3& GetTranslate() const { return transform_.translate; }

private: // メンバ構造体
	
	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	struct DirectionalLight {
		MatrixMath::Vector4 color; //!< ライトの色
		MatrixMath::Vector3 direction; //!< ライトの向き
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
	
	MatrixMath::Transform transform_;
	MatrixMath::Transform cameraTransform_;

	Model* model_ = nullptr;
};


