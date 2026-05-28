#define NOMINMAX
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

	float Dot3(const Vector3& a, const Vector3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
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

CollisionUtility::RayHitResult Reticle::CastCameraAimRay(float ignoreBeforeT) const
{
	CollisionUtility::RayHitResult best{};
	best.t = std::numeric_limits<float>::infinity();

	if (!camera_ || !cameraController_ || !blockColliders_) {
		return best;
	}

	CollisionUtility::Ray ray;
	ray.origin = camera_->GetTranslate();
	ray.dir = Normalize3(cameraController_->GetForwardDirection());

	for (const auto& obb : *blockColliders_) {
		CollisionUtility::RayHitResult hit =
			CollisionUtility::RayIntersectOBB_Detailed(ray, obb);

		if (!hit.hit || hit.t < 0.0f || hit.t > aimMaxDistance_) {
			continue;
		}

		// カメラとプレイヤーの間にあるブロックは、狙い補正には使わない
		if (hit.t < ignoreBeforeT) {
			continue;
		}

		if (hit.t < best.t) {
			best = hit;
		}
	}

	return best;
}

float Reticle::GetCameraRayPlayerPassT() const
{
	if (!camera_ || !cameraController_ || !player_) {
		return 0.0f;
	}

	const Vector3 rayOrigin = camera_->GetTranslate();
	const Vector3 rayDir = Normalize3(cameraController_->GetForwardDirection());

	// 実際の舌は口元から飛ぶので、プレイヤー中心より口元を基準にする
	Vector3 playerReference = player_->GetPosition();

	if (Tongue* tongue = player_->GetTongue()) {
		playerReference = tongue->GetMouthWorldPositionPublic();
	}

	const Vector3 toPlayer = playerReference - rayOrigin;
	const float t = Dot3(toPlayer, rayDir);

	return std::max(0.0f, t);
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

	const float playerPassT = GetCameraRayPlayerPassT();

	// プレイヤー付近に到達する前のヒットは無視する。
	// 少し手前から許可しておくと、カメラRayと口元が完全一致しない時も破綻しにくい。
	const float ignoreBeforeT =
		std::max(0.0f, playerPassT - cameraPlayerPassMargin_);

	CollisionUtility::RayHitResult cameraHit =
		CastCameraAimRay(ignoreBeforeT);

	Vector3 maxDistanceTarget =
		camera_->GetTranslate() + Normalize3(cameraController_->GetForwardDirection()) * aimMaxDistance_;

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

			constexpr float kShotStartForwardOffset = 0.15f;
			constexpr float kTongueHitRadiusMargin = 0.30f;

			canReachTongueTarget_ =
				(distance <= tongue->GetMaxDistance() + kShotStartForwardOffset + kTongueHitRadiusMargin);
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