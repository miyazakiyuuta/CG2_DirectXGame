#pragma once

#include "io/Input.h"
#include "math/Vector3.h"
#include <algorithm>

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
	}

	void SetHeight(float height){ height_ = height; }

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
	}

	void SetZoomStep(float step){ zoomStep_ = step; }
	void SetZoomEaseSpeed(float speed){ zoomEaseSpeed_ = speed; }

	float GetYaw() const{ return yaw_; }
	float GetPitch() const{ return pitch_; }
	float GetDistance() const{ return distance_; }

	bool SetIsUse(bool isUse){ return isUse_ = isUse; }
	bool GetIsUse() const{ return isUse_; }

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
	float minZoom_ = 3.0f;
	float maxZoom_ = 30.0f;
	float zoomStep_ = 0.02f;
	float zoomEaseSpeed_ = 0.2f;

	Vector3 targetOffset_ = { 0.0f, 1.0f, 0.0f };

	bool isUse_ = false;
};