#pragma once

#include "engine/base/Matrix4x4.h"
#include "engine/base/Vector3.h"

// カメラ
class Camera {
public: // メンバ関数
	Camera();
	void Update();

public:
	// setter
	void SetRotate(const MatrixMath::Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const MatrixMath::Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }
	// getter
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	MatrixMath::Vector3& GetRotate() { return transform_.rotate; }
	MatrixMath::Vector3& GetTranslate() { return transform_.translate; }

private:
	MatrixMath::Transform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
	Matrix4x4 viewProjectionMatrix_;
};

