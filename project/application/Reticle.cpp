#include "Reticle.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "CameraController.h"
#include "Player.h"
#include "Tongue.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace{
	float Length3(const Vector3& v){
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 Normalize3(const Vector3& v){
		float len = Length3(v);
		if(len <= 0.0001f){
			return { 0.0f, 0.0f, 1.0f };
		}
		return { v.x / len, v.y / len, v.z / len };
	}
}

void Reticle::Initialize(
	SpriteCommon* spriteCommon,
	Camera* camera,
	CameraController* cameraController,
	Player* player,
	const std::vector<CollisionUtility::OBB>* blockColliders
){
	spriteCommon_ = spriteCommon;
	camera_ = camera;
	cameraController_ = cameraController;
	player_ = player;
	blockColliders_ = blockColliders;

	centerDot_ = std::make_unique<Sprite>();
	centerDot_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	centerDot_->SetAnchorPoint({ 0.5f, 0.5f });

	barUp_ = std::make_unique<Sprite>();
	barUp_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	barUp_->SetAnchorPoint({ 0.5f, 0.5f });

	barDown_ = std::make_unique<Sprite>();
	barDown_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	barDown_->SetAnchorPoint({ 0.5f, 0.5f });

	barLeft_ = std::make_unique<Sprite>();
	barLeft_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	barLeft_->SetAnchorPoint({ 0.5f, 0.5f });

	barRight_ = std::make_unique<Sprite>();
	barRight_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	barRight_->SetAnchorPoint({ 0.5f, 0.5f });

	SetColor(color_);
	ApplyLayout();
	UpdateVisual();
	UpdateAimTarget();
}

void Reticle::SetColor(const Vector4& color) {
	normalColor_ = color;
	color_ = color;

	if (centerDot_) centerDot_->SetColor(color_);
	if (barUp_)     barUp_->SetColor(color_);
	if (barDown_)   barDown_->SetColor(color_);
	if (barLeft_)   barLeft_->SetColor(color_);
	if (barRight_)  barRight_->SetColor(color_);
}

void Reticle::ApplyLayout(){
	if(!centerDot_ || !barUp_ || !barDown_ || !barLeft_ || !barRight_){
		return;
	}

	const float scaledCenterSize = centerSize_ * scale_;
	const float scaledSpread = spread_ * scale_;

	const float scaledBarWidth = barWidth_ * scale_;
	const float scaledBarHeight = barHeight_ * scale_;

	const float scaledBarLengthVertical = barLengthVertical_ * scale_;
	const float scaledBarThicknessVertical = barThicknessVertical_ * scale_;

	centerDot_->SetPos(center_);
	centerDot_->SetSize({ scaledCenterSize, scaledCenterSize });

	barUp_->SetPos({ center_.x, center_.y - scaledSpread });
	barUp_->SetSize({ scaledBarThicknessVertical, scaledBarLengthVertical });

	barDown_->SetPos({ center_.x, center_.y + scaledSpread });
	barDown_->SetSize({ scaledBarThicknessVertical, scaledBarLengthVertical });

	barLeft_->SetPos({ center_.x - scaledSpread, center_.y });
	barLeft_->SetSize({ scaledBarWidth, scaledBarHeight });

	barRight_->SetPos({ center_.x + scaledSpread, center_.y });
	barRight_->SetSize({ scaledBarWidth, scaledBarHeight });
}

void Reticle::UpdateVisual(){
	if(!isVisible_){
		return;
	}

	ApplyLayout();

	centerDot_->Update();
	barUp_->Update();
	barDown_->Update();
	barLeft_->Update();
	barRight_->Update();
}

CollisionUtility::RayHitResult Reticle::CastCameraAimRay() const{
	CollisionUtility::RayHitResult best{};
	best.t = std::numeric_limits<float>::infinity();

	if(!camera_ || !cameraController_ || !blockColliders_){
		return best;
	}

	CollisionUtility::Ray ray;
	ray.origin = camera_->GetTranslate();
	ray.dir = cameraController_->GetForwardDirection();

	for(const auto& obb : *blockColliders_){
		CollisionUtility::RayHitResult hit =
			CollisionUtility::RayIntersectOBB_Detailed(ray, obb);

		if(!hit.hit || hit.t < 0.0f || hit.t > aimMaxDistance_){
			continue;
		}

		if(hit.t < best.t){
			best = hit;
		}
	}

	return best;
}

CollisionUtility::RayHitResult Reticle::CastPlayerAimRay(const Vector3& targetPoint) const{
	CollisionUtility::RayHitResult best{};
	best.t = std::numeric_limits<float>::infinity();

	if(!player_ || !blockColliders_){
		return best;
	}

	Tongue* tongue = player_->GetTongue();
	Vector3 origin = player_->GetPosition();

	if(tongue){
		origin = tongue->GetMouthWorldPositionPublic();
	} else{
		origin.y += 1.0f;
	}

	Vector3 dir = targetPoint - origin;
	float maxDistance = Length3(dir);
	if(maxDistance <= 0.0001f){
		return best;
	}

	CollisionUtility::Ray ray;
	ray.origin = origin;
	ray.dir = dir;

	for(const auto& obb : *blockColliders_){
		CollisionUtility::RayHitResult hit =
			CollisionUtility::RayIntersectOBB_Detailed(ray, obb);

		if(!hit.hit || hit.t < 0.0f || hit.t > maxDistance){
			continue;
		}

		if(hit.t < best.t){
			best = hit;
		}
	}

	return best;
}

void Reticle::UpdateAimTarget() {
	hasAimTargetPoint_ = false;
	canReachTongueTarget_ = false;

	if(!camera_ || !cameraController_){
		color_ = normalColor_;
		if (centerDot_) centerDot_->SetColor(color_);
		if (barDown_)   barDown_->SetColor(color_);
		if (barLeft_)   barLeft_->SetColor(color_);
		if (barRight_)  barRight_->SetColor(color_);
		return;
	}

	CollisionUtility::RayHitResult cameraHit = CastCameraAimRay();

	Vector3 maxDistanceTarget =
		camera_->GetTranslate() + cameraController_->GetForwardDirection() * aimMaxDistance_;

	Vector3 cameraTargetPoint = cameraHit.hit ? cameraHit.point : maxDistanceTarget;

	CollisionUtility::RayHitResult playerHit = CastPlayerAimRay(cameraTargetPoint);

	aimTargetPoint_ = playerHit.hit ? playerHit.point : cameraTargetPoint;
	hasAimTargetPoint_ = true;

	// ブロックに実際に当たる点があり、かつ舌が届くときだけ赤
	if (player_ && playerHit.hit) {
		Tongue* tongue = player_->GetTongue();
		if (tongue) {
			Vector3 mouthPos = tongue->GetMouthWorldPositionPublic();
			Vector3 toHit = playerHit.point - mouthPos;

			float distance = toHit.Length();
			canReachTongueTarget_ = (distance <= tongue->GetMaxDistance());
		}
	}

	color_ = canReachTongueTarget_ ? reachableColor_ : normalColor_;

	if (centerDot_) centerDot_->SetColor(color_);
	if (barUp_)     barUp_->SetColor(color_);
	if (barDown_)   barDown_->SetColor(color_);
	if (barLeft_)   barLeft_->SetColor(color_);
	if (barRight_)  barRight_->SetColor(color_);
}

void Reticle::Update(){
	UpdateAimTarget();
	UpdateVisual();
}

void Reticle::Draw(){
	if(!isVisible_){
		return;
	}

	if(centerDot_) centerDot_->Draw();
	if(barUp_)     barUp_->Draw();
	if(barDown_)   barDown_->Draw();
	if(barLeft_)   barLeft_->Draw();
	if(barRight_)  barRight_->Draw();
}