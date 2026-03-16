#include "CameraController.h"
#include "3d/Camera.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

void CameraController::Initialize(Camera* camera){
	camera_ = camera;
	input_ = Input::GetInstance();

	// 初期値を整合
	targetDistance_ = distance_;
	distance_ = std::clamp(distance_, minZoom_, maxZoom_);
	targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);

	if(camera_){
		camera_->SetRotate({ pitch_, yaw_, 0.0f });
	}

	isUse_ = false;
}

void CameraController::Update(const Vector3& target){
	if(!camera_ || !input_ || !isUse_){
		return;
	}

	// キーボード操作
	if(input_->IsPushKey(DIK_LEFT)){
		yaw_ -= yawSpeed_;
	}
	if(input_->IsPushKey(DIK_RIGHT)){
		yaw_ += yawSpeed_;
	}
	if(input_->IsPushKey(DIK_UP)){
		pitch_ += pitchSpeed_;
	}
	if(input_->IsPushKey(DIK_DOWN)){
		pitch_ -= pitchSpeed_;
	}

	// マウス操作
	// 右クリック中だけ回転する想定
	// GetMouseMoveX / GetMouseMoveY が Input 側にある前提
	if(!ImGui::GetIO().WantCaptureMouse){
		const int mouseMoveX = input_->GetMouseMove().x;
		const int mouseMoveY = -input_->GetMouseMove().y;

		yaw_ += static_cast<float>(mouseMoveX) * mouseSensitivity_;

		const float ySign = invertY_ ? 1.0f : -1.0f;
		pitch_ += static_cast<float>(mouseMoveY) * mouseSensitivity_ * ySign;
	}

	// ホイール入力は目標距離だけを変える
	const int wheel = input_->GetMouseWheel();
	if(wheel != 0){
		targetDistance_ -= static_cast<float>(wheel) * zoomStep_;
		targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);
	}

	// 現在距離を目標距離へなめらかに近づける
	distance_ += (targetDistance_ - distance_) * zoomEaseSpeed_;
	distance_ = std::clamp(distance_, minZoom_, maxZoom_);

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

void CameraController::DrawImGui(){
	if(ImGui::TreeNode("CameraController")){
		ImGui::Checkbox("Use CameraController", &isUse_);

		ImGui::SliderFloat("Distance", &targetDistance_, minZoom_, maxZoom_);
		ImGui::SliderFloat("Min Zoom", &minZoom_, 0.5f, 30.0f);
		ImGui::SliderFloat("Max Zoom", &maxZoom_, 1.0f, 60.0f);
		ImGui::SliderFloat("Zoom Ease", &zoomEaseSpeed_, 0.01f, 1.0f);
		ImGui::SliderFloat("Zoom Step", &zoomStep_, 0.0001f, 0.1f, "%.4f");

		ImGui::Separator();
		ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity_, 0.001f, 0.05f, "%.4f");
		ImGui::Checkbox("Invert Y", &invertY_);

		if(minZoom_ > maxZoom_){
			std::swap(minZoom_, maxZoom_);
		}
		targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);

		ImGui::Text("Mouse: Right Drag Rotate / Wheel Zoom");

		ImGui::TreePop();
	}
}