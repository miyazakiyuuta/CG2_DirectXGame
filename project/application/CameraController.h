#pragma once

#define NOMINMAX
#include "io/Input.h"
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"
#include <algorithm>
#include <vector>

class Camera;

class CameraController{
public:
	void Initialize(Camera* camera);

	// プレイヤー位置を中心に、独立した yaw / pitch で回る
	void Update(const Vector3& target);

	void SetTargetOffset(const Vector3& offset){ targetOffset_ = offset; }

	void SetDistance(float distance){
		distance_ = distance;
		targetDistance_ = distance;
		normalDistance_ = distance;
	}

	void SetHeight(float height){
		height_ = height;
		normalHeight_ = height;
	}

	void SetYawSpeed(float speed){ yawSpeed_ = speed; }
	void SetPitchSpeed(float speed){ pitchSpeed_ = speed; }

	void SetZoomRange(float minZoom, float maxZoom){
		minZoom_ = minZoom;
		maxZoom_ = maxZoom;
		if(minZoom_ > maxZoom_){
			std::swap(minZoom_, maxZoom_);
		}
		targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);
		distance_ = std::clamp(distance_, minZoom_, maxZoom_);
		normalDistance_ = std::clamp(normalDistance_, minZoom_, maxZoom_);
	}

	void SetZoomStep(float step){ zoomStep_ = step; }
	void SetZoomEaseSpeed(float speed){ zoomEaseSpeed_ = speed; }

	void SetMouseSensitivity(float sensitivity){ mouseSensitivity_ = sensitivity; }
	void SetInvertY(bool invert){ invertY_ = invert; }

	// 遮蔽物として使うブロック群
	void SetObstacleColliders(const std::vector<CollisionUtility::OBB>* obstacleColliders){
		obstacleColliders_ = obstacleColliders;
	}

	// ヒット位置の手前にどれだけ余白を残すか
	void SetCameraCollisionMargin(float margin){ cameraCollisionMargin_ = margin; }

	float GetYaw() const{ return yaw_; }
	float GetPitch() const{ return pitch_; }
	float GetDistance() const{ return distance_; }

	bool SetIsUse(bool isUse){ return isUse_ = isUse; }
	bool GetIsUse() const{ return isUse_; }

	// 右クリック中の一人称寄り視点
	bool IsAimMode() const{ return isAimMode_; }
	Vector3 GetForwardDirection() const;

	void DrawImGui();

	void SetKeepInsideCylinder(const CollisionUtility::Cylinder* cylinder){
		keepInsideCylinder_ = cylinder;
	}

	void SetObstacleCylinder(const CollisionUtility::Cylinder* cylinder){
		obstacleCylinder_ = cylinder;
	}

private:
	Camera* camera_ = nullptr;
	Input* input_ = nullptr;

	float yaw_ = 0.0f;
	float pitch_ = 0.35f;

	// 実際の表示距離
	float distance_ = 10.0f;
	// ホイール入力で変わる目標距離
	float targetDistance_ = 10.0f;

	float height_ = 2.5f;

	float yawSpeed_ = 0.03f;
	float pitchSpeed_ = 0.02f;

	// ズーム設定
	float minZoom_ = 1.0f;
	float maxZoom_ = 30.0f;
	float zoomStep_ = 0.02f;
	float zoomEaseSpeed_ = 0.2f;

	// マウス回転設定
	float mouseSensitivity_ = 0.01f;
	bool invertY_ = false;

	Vector3 targetOffset_ = { 0.0f, 1.0f, 0.0f };

	// 通常時のカメラ値
	float normalDistance_ = 10.0f;
	float normalHeight_ = 2.5f;

	// 右クリック中の一人称寄りカメラ値
	bool isAimMode_ = false;
	float aimDistance_ = -0.20f;
	float aimHeight_ = 0.10f;
	Vector3 aimTargetOffset_ = { 0.0f, 1.35f, 0.0f };

	// カメラ遮蔽対策
	const std::vector<CollisionUtility::OBB>* obstacleColliders_ = nullptr;
	float cameraCollisionMargin_ = 0.50f;

	bool isUse_ = true;

	Vector3 currentForward_ = { 0.0f, 0.0f, 1.0f };

	float cameraBodyRadius_ = 0.05f;

	float cameraCollisionMinDistance_ = 0.15f;

	const CollisionUtility::Cylinder* obstacleCylinder_ = nullptr;
	const CollisionUtility::Cylinder* keepInsideCylinder_ = nullptr;
};