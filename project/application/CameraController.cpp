#include "CameraController.h"

#include "3d/Camera.h"
#include <algorithm>
#include <cmath>

void CameraController::Initialize(Camera* camera){
	camera_ = camera;
	input_ = Input::GetInstance();

	if(camera_){
		camera_->SetRotate({ pitch_, yaw_, 0.0f });
	}
}

void CameraController::Update(const Vector3& target){
	if(!camera_ || !input_){
		return;
	}

	// 左右で水平方向の回転
	if(input_->IsPushKey(DIK_LEFT)){
		yaw_ -= yawSpeed_;
	}
	if(input_->IsPushKey(DIK_RIGHT)){
		yaw_ += yawSpeed_;
	}

	// 上下で見下ろし角を調整
	if(input_->IsPushKey(DIK_UP)){
		pitch_ += pitchSpeed_;
	}
	if(input_->IsPushKey(DIK_DOWN)){
		pitch_ -= pitchSpeed_;
	}

	// 行き過ぎ防止
	const float kMinPitch = 0.10f;
	const float kMaxPitch = 1.10f;
	pitch_ = std::clamp(pitch_, kMinPitch, kMaxPitch);

	Vector3 focus = {
		target.x + targetOffset_.x,
		target.y + targetOffset_.y,
		target.z + targetOffset_.z
	};

	float cosPitch = std::cos(pitch_);
	float sinPitch = std::sin(pitch_);
	float sinYaw = std::sin(yaw_);
	float cosYaw = std::cos(yaw_);

	// ターゲットの周囲をギズモカメラ風に回る
	Vector3 offset = {
		-distance_ * sinYaw * cosPitch,
		 distance_ * sinPitch + height_,
		-distance_ * cosYaw * cosPitch
	};

	Vector3 cameraPos = {
		focus.x + offset.x,
		focus.y + offset.y,
		focus.z + offset.z
	};

	camera_->SetTranslate(cameraPos);
	camera_->SetRotate({ pitch_, yaw_, 0.0f });
}