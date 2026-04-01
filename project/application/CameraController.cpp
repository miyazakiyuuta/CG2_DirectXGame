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

	Vector3 CrossVec(const Vector3& a, const Vector3& b){
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	float DotVec(const Vector3& a, const Vector3& b){
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	Vector3 ClosestPointOnOBB(const Vector3& point, const CollisionUtility::OBB& obb){
		Vector3 d = point - obb.center;
		Vector3 result = obb.center;

		for(int i = 0; i < 3; ++i){
			float dist = DotVec(d, obb.axis[i]);
			dist = std::clamp(dist, -obb.halfLength[i], obb.halfLength[i]);
			result += obb.axis[i] * dist;
		}

		return result;
	}
}

void CameraController::Initialize(Camera* camera){
	camera_ = camera;
	input_ = Input::GetInstance();

	// 初期値を整合
	targetDistance_ = distance_;
	distance_ = std::clamp(distance_, minZoom_, maxZoom_);
	targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);
	normalDistance_ = distance_;
	normalHeight_ = height_;

	if(camera_){
		camera_->SetRotate({ pitch_, yaw_, 0.0f });
	}

	isUse_ = true;
}

Vector3 CameraController::GetForwardDirection() const{
	Vector3 forward = {
		std::sin(yaw_) * std::cos(pitch_),
		-std::sin(pitch_),
		std::cos(yaw_) * std::cos(pitch_)
	};
	return NormalizeVecSafe(forward);
}

void CameraController::Update(const Vector3& target){
	if(!camera_ || !input_ || !isUse_){
		return;
	}

	// 右クリック中は一人称寄りのエイム視点
	isAimMode_ = input_->IsPressMouse(1);

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

		const int mouseMoveX = input_->GetMouseMove().x;
		const int mouseMoveY = -input_->GetMouseMove().y;

		yaw_ += static_cast<float>(mouseMoveX) * mouseSensitivity_;

		const float ySign = invertY_ ? 1.0f : -1.0f;
		pitch_ += static_cast<float>(mouseMoveY) * mouseSensitivity_ * ySign;

	// ホイール入力は通常時だけ反映
	if(!isAimMode_){
		const int wheel = input_->GetMouseWheel();
		if(wheel != 0){
			targetDistance_ -= static_cast<float>(wheel) * zoomStep_;
			targetDistance_ = std::clamp(targetDistance_, minZoom_, maxZoom_);
		}
	}

	// 現在距離を目標距離へなめらかに近づける
	const float desiredDistance = isAimMode_ ? aimDistance_ : targetDistance_;
	distance_ += (desiredDistance - distance_) * zoomEaseSpeed_;

	const float clampMin = isAimMode_ ? -0.50f : minZoom_;
	const float clampMax = isAimMode_ ? 0.30f : maxZoom_;
	distance_ = std::clamp(distance_, clampMin, clampMax);

	// 行き過ぎ防止
	const float kMinPitch = -3.14f / 2.0f;
	const float kMaxPitch = 3.14f / 2.0f;
	pitch_ = std::clamp(pitch_, kMinPitch, kMaxPitch);

	Vector3 focus = target;
	float usingHeight = height_;

	if(isAimMode_){
		focus.x += aimTargetOffset_.x;
		focus.y += aimTargetOffset_.y;
		focus.z += aimTargetOffset_.z;
		usingHeight = aimHeight_;
	} else{
		focus.x += targetOffset_.x;
		focus.y += targetOffset_.y;
		focus.z += targetOffset_.z;
		usingHeight = normalHeight_;
	}

	float cosPitch = std::cos(pitch_);
	float sinPitch = std::sin(pitch_);
	float sinYaw = std::sin(yaw_);
	float cosYaw = std::cos(yaw_);

	Vector3 offset = {
		-distance_ * sinYaw * cosPitch,
		 distance_ * sinPitch + usingHeight,
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
			Vector3 dir = NormalizeVecSafe(rayDir);

			Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
			Vector3 right = NormalizeVecSafe(CrossVec(worldUp, dir));
			Vector3 up = NormalizeVecSafe(CrossVec(dir, right));

			// 上下方向を少し強めに見る
			const float horizontalProbe = 0.01f;
			const float verticalProbe = 0.01f;

			Vector3 rayOrigins[] = {
				focus,
				focus + up * verticalProbe,
				focus - up * verticalProbe,
				focus + right * horizontalProbe,
				focus - right * horizontalProbe,
			};

			float nearestT = std::numeric_limits<float>::infinity();
			bool hitSomething = false;

			for(const auto& origin : rayOrigins){
				CollisionUtility::Ray ray;
				ray.origin = origin;
				ray.dir = dir;

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

				if(obstacleCylinder_){
					CollisionUtility::RayHitResult hit =
						CollisionUtility::RayIntersectCylinder_Detailed(ray, *obstacleCylinder_);

					if(hit.hit && hit.t >= 0.0f && hit.t <= desiredLength){
						if(hit.t < nearestT){
							nearestT = hit.t;
							hitSomething = true;
						}
					}
				}
			}

			if(hitSomething){
				float minAllowed = isAimMode_ ? -0.50f : cameraCollisionMinDistance_;

				const float extraPushBack = 0.08f;
				float safeT = std::max(minAllowed, nearestT - cameraCollisionMargin_ - extraPushBack);

				finalCameraPos = {
					focus.x + dir.x * safeT,
					focus.y + dir.y * safeT,
					focus.z + dir.z * safeT
				};
			}
		}
	}

	if(obstacleColliders_){
		for(int iteration = 0; iteration < 3; ++iteration){
			bool pushed = false;

			for(const auto& box : *obstacleColliders_){
				Vector3 closest = ClosestPointOnOBB(finalCameraPos, box);
				Vector3 diff = finalCameraPos - closest;

				float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
				float radiusSq = cameraBodyRadius_ * cameraBodyRadius_;

				if(distSq >= radiusSq){
					continue;
				}

				float dist = std::sqrt(std::max(distSq, 0.000001f));
				Vector3 pushDir;

				if(dist > 0.0001f){
					pushDir = diff * (1.0f / dist);

					// 上下方向を少し優先して逃がす
					if(std::abs(pushDir.y) < 0.6f){
						pushDir.y += (finalCameraPos.y < focus.y) ? 0.6f : -0.6f;
						pushDir = NormalizeVecSafe(pushDir);
					}
				} else{
					pushDir = (finalCameraPos.y < focus.y)
						? Vector3{ 0.0f, 1.0f, 0.0f }
					: Vector3{ 0.0f, -1.0f, 0.0f };
				}

				float penetration = cameraBodyRadius_ - dist + 0.01f;
				finalCameraPos += pushDir * penetration;
				pushed = true;
			}

			if(!pushed){
				break;
			}
		}
	}

	if(keepInsideCylinder_){
		finalCameraPos = CollisionUtility::ClosestPointInsideCylinder(
			finalCameraPos,
			*keepInsideCylinder_
		);
	}

	currentForward_ = {
		focus.x - finalCameraPos.x,
		focus.y - finalCameraPos.y,
		focus.z - finalCameraPos.z
	};
	currentForward_ = NormalizeVecSafe(currentForward_);

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

		ImGui::Separator();
		ImGui::Text("Aim Camera");
		ImGui::Text("Hold Right Mouse Button");
		ImGui::SliderFloat("Aim Distance", &aimDistance_, -1.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Aim Height", &aimHeight_, -1.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Aim Target Offset", &aimTargetOffset_.x, 0.01f);

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