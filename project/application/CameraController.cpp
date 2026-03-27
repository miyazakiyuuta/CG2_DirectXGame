#include "CameraController.h"
#include "3d/Camera.h"
#include <algorithm>
#include <cmath>
#include <limits>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace{
	float LengthVec(const Vector3& v){
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 NormalizeVecSafe(const Vector3& v){
		float len = LengthVec(v);
		if(len <= 0.0001f){
			return { 0.0f, 0.0f, -1.0f };
		}
		return { v.x / len, v.y / len, v.z / len };
	}
}

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

	isUse_ = true;
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

#ifdef USE_IMGUI
	// マウス操作
	if(!ImGui::GetIO().WantCaptureMouse){
		const int mouseMoveX = input_->GetMouseMove().x;
		const int mouseMoveY = -input_->GetMouseMove().y;

		yaw_ += static_cast<float>(mouseMoveX) * mouseSensitivity_;

		const float ySign = invertY_ ? 1.0f : -1.0f;
		pitch_ += static_cast<float>(mouseMoveY) * mouseSensitivity_ * ySign;
	}
#endif

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
	const float kMinPitch = -1.10f;
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

	// 本来置きたい理想位置
	Vector3 desiredCameraPos = {
		focus.x + offset.x,
		focus.y + offset.y,
		focus.z + offset.z
	};

	// 遮蔽物があるなら、手前までカメラを寄せる
	Vector3 finalCameraPos = desiredCameraPos;

	if(obstacleColliders_){
		Vector3 rayDir = {
			desiredCameraPos.x - focus.x,
			desiredCameraPos.y - focus.y,
			desiredCameraPos.z - focus.z
		};

		float desiredLength = LengthVec(rayDir);
		if(desiredLength > 0.0001f){
			CollisionUtility::Ray ray;
			ray.origin = focus;
			ray.dir = NormalizeVecSafe(rayDir);

			float nearestT = std::numeric_limits<float>::infinity();
			bool hitSomething = false;

			for(const auto& box : *obstacleColliders_){
				CollisionUtility::RayHitResult hit =
					CollisionUtility::RayIntersectOBB_Detailed(ray, box);

				if(!hit.hit){
					continue;
				}

				if(hit.t < 0.0f || hit.t > desiredLength){
					continue;
				}

				if(hit.t < nearestT){
					nearestT = hit.t;
					hitSomething = true;
				}
			}

			if(hitSomething){
				float safeT = std::max(minZoom_, nearestT - cameraCollisionMargin_);
				finalCameraPos = {
					focus.x + ray.dir.x * safeT,
					focus.y + ray.dir.y * safeT,
					focus.z + ray.dir.z * safeT
				};
			}
		}
	}

	camera_->SetTranslate(finalCameraPos);
	camera_->SetRotate({ pitch_, yaw_, 0.0f });
}

void CameraController::DrawImGui(){
#ifdef USE_IMGUI
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

		ImGui::Separator();
		ImGui::SliderFloat("Camera Collision Margin", &cameraCollisionMargin_, 0.01f, 1.0f, "%.2f");

		if(minZoom_ > maxZoom_){
			std::swap(minZoom_, maxZoom_);
		}
		targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);

		ImGui::Text("Mouse: Drag Rotate / Wheel Zoom");
		ImGui::Text("If a block comes between player and camera,");
		ImGui::Text("camera moves forward until the block leaves view.");

		ImGui::TreePop();
	}
#endif
}