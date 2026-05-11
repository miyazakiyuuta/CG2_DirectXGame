#pragma once

#define NOMINMAX
#include "io/Input.h"
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"
#include <algorithm>
#include <vector>

class Camera;

class CameraController {
public:
	void Initialize(Camera* camera);

	// プレイヤー位置を中心に、独立した yaw / pitch で回る
	void Update(const Vector3& target);

	void SetTargetOffset(const Vector3& offset) { targetOffset_ = offset; }

	void SetDistance(float distance) {
		distance_ = distance;
		targetDistance_ = distance;
		normalDistance_ = distance;
	}

	void SetHeight(float height) {
		height_ = height;
		normalHeight_ = height;
	}

	// 各種移動速度・感度の設定
	void SetYawSpeed(float speed) { yawSpeed_ = speed; }
	void SetPitchSpeed(float speed) { pitchSpeed_ = speed; }
	void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

	// ベース速度のGetter/Setter (マジックナンバー排除用)
	float GetBaseYawSpeed() const { return baseYawSpeed_; }
	float GetBasePitchSpeed() const { return basePitchSpeed_; }
	float GetBaseMouseSensitivity() const { return baseMouseSensitivity_; }
	float GetBasePadYawSpeed() const { return basePadYawSpeed_; }
	float GetBasePadPitchSpeed() const { return basePadPitchSpeed_; }

	void SetBaseYawSpeed(float speed) { baseYawSpeed_ = speed; }
	void SetBasePitchSpeed(float speed) { basePitchSpeed_ = speed; }
	void SetBaseMouseSensitivity(float sensitivity) { baseMouseSensitivity_ = sensitivity; }

	void SetZoomRange(float minZoom, float maxZoom) {
		minZoom_ = minZoom;
		maxZoom_ = maxZoom;
		if (minZoom_ > maxZoom_) {
			std::swap(minZoom_, maxZoom_);
		}
		targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);
		distance_ = std::clamp(distance_, minZoom_, maxZoom_);
		normalDistance_ = std::clamp(normalDistance_, minZoom_, maxZoom_);
	}

	void SetZoomStep(float step) { zoomStep_ = step; }
	void SetZoomEaseSpeed(float speed) { zoomEaseSpeed_ = speed; }

	void SetInvertY(bool invert) { invertY_ = invert; }

	// 遮蔽物として使うブロック群
	void SetObstacleColliders(const std::vector<CollisionUtility::OBB>* obstacleColliders) { obstacleColliders_ = obstacleColliders; }

	void SetCameraCollisionMargin(float margin) { cameraCollisionMargin_ = margin; }
	void SetEnableTopViewOcclusionRelax(bool enable) { enableTopViewOcclusionRelax_ = enable; }

	void SetTopViewRelaxAngleRange(float minAngleDeg, float maxAngleDeg) {
		topViewRelaxMinAngleDeg_ = std::clamp(minAngleDeg, 0.0f, 180.0f);
		topViewRelaxMaxAngleDeg_ = std::clamp(maxAngleDeg, 0.0f, 180.0f);
		if (topViewRelaxMinAngleDeg_ > topViewRelaxMaxAngleDeg_) {
			std::swap(topViewRelaxMinAngleDeg_, topViewRelaxMaxAngleDeg_);
		}
	}

	void SetTopViewDisablePullIn(bool disable) { topViewDisablePullIn_ = disable; }

	void SetTopViewOcclusionMarginScale(float scale) { topViewOcclusionMarginScale_ = std::clamp(scale, 0.0f, 1.0f); }

	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }
	float GetDistance() const { return distance_; }
	float GetMouseSensitivity() const { return mouseSensitivity_; }

	bool SetIsUse(bool isUse) { return isUse_ = isUse; }
	bool GetIsUse() const { return isUse_; }

	bool IsAimMode() const { return isAimMode_; }
	Vector3 GetForwardDirection() const;

	void DrawImGui();

	void SetKeepInsideCylinder(const CollisionUtility::Cylinder* cylinder) { keepInsideCylinder_ = cylinder; }

	void SetObstacleCylinder(const CollisionUtility::Cylinder* cylinder) { obstacleCylinder_ = cylinder; }

private:
	Camera* camera_ = nullptr;
	Input* input_ = nullptr;

	float yaw_ = 0.0f;
	float pitch_ = 0.35f;

	float distance_ = 10.0f;
	float targetDistance_ = 10.0f;
	float height_ = 2.5f;

	// 現在の適用速度
	float yawSpeed_ = 0.03f;
	float pitchSpeed_ = 0.02f;
	float mouseSensitivity_ = 0.01f;

	// ベース速度設定 (ここを基準に感度倍率をかける)
	float baseYawSpeed_ = 0.03f;
	float basePitchSpeed_ = 0.02f;
	float baseMouseSensitivity_ = 0.005f; // デフォルト基準値
	float basePadYawSpeed_ = 0.08f;
	float basePadPitchSpeed_ = 0.06f;

	// ズーム設定
	float minZoom_ = 1.0f;
	float maxZoom_ = 30.0f;
	float zoomStep_ = 0.02f;
	float zoomEaseSpeed_ = 0.2f;

	bool invertY_ = false;

	Vector3 targetOffset_ = {0.0f, 1.0f, 0.0f};
	float normalDistance_ = 10.0f;
	float normalHeight_ = 2.5f;

	bool isAimMode_ = false;
	float aimDistance_ = -0.20f;
	float aimHeight_ = 0.10f;
	Vector3 aimTargetOffset_ = {0.0f, 1.35f, 0.0f};

	const std::vector<CollisionUtility::OBB>* obstacleColliders_ = nullptr;
	float cameraCollisionMargin_ = 0.50f;
	float topViewRelaxMinAngleDeg_ = 0.0f;
	float topViewRelaxMaxAngleDeg_ = 90.0f;
	bool topViewDisablePullIn_ = true;
	bool enableTopViewOcclusionRelax_ = true;
	float topViewRelaxStartDot_ = 0.70f;
	float topViewRelaxEndDot_ = 0.85f;
	float topViewOcclusionMarginScale_ = 0.35f;

	bool isUse_ = true;
	Vector3 currentForward_ = {0.0f, 0.0f, 1.0f};
	float cameraBodyRadius_ = 0.05f;
	float cameraCollisionMinDistance_ = 0.15f;

	const CollisionUtility::Cylinder* obstacleCylinder_ = nullptr;
	const CollisionUtility::Cylinder* keepInsideCylinder_ = nullptr;

	// Debug members...
	float debugTopAngleDeg_ = 0.0f;
	bool debugIsInsideTopRelaxAngle_ = false;
	bool debugHitSomething_ = false;
	bool debugShouldRelax_ = false;
	bool debugActuallyRelaxed_ = false;
	bool debugActuallySkippedPullIn_ = false;
	float debugNearestT_ = -1.0f;
	float debugEffectiveMargin_ = 0.0f;
	float debugSafeT_ = -1.0f;
	bool debugHasObstacleColliders_ = false;
	bool debugIsAimMode_ = false;
};