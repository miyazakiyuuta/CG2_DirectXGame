#include "SentinelEnemy.h"
#include "../../../application/Player.h"
#include "../../../engine/3d/Object3d.h"
#include <algorithm>
#include <cmath>

SentinelEnemy::SentinelEnemy()
    : homePosition_({0, 0, 0}), state_(State::Idle), floatTimer_(0.0f), normalSpeed_(3.0f), fleeSpeed_(12.0f), panicSpeed_(65.0f) // 【さらに強化】スリングショットの爽快感のために最高速度をアップ
      ,
      detectRange_(20.0f), loseRange_(40.0f), panicTimer_(0.0f) {}

SentinelEnemy::~SentinelEnemy() = default;

void SentinelEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("SentinelHook.obj");
	object_->SetCamera(camera);

	position_ = pos;
	homePosition_ = pos;
	object_->SetScale({0.7f, 0.7f, 0.7f});
	object_->SetColor({0.0f, 1.0f, 0.5f, 1.0f}); // 通常：緑

	gravity_ = -0.01f;
	groundY_ = -100.0f;
}

// 舌がヒットした瞬間の通知
void SentinelEnemy::OnTongueHit(const Vector3& direction) {
	if (state_ != State::Panicking) {
		state_ = State::Panicking;
		if(direction.Length() > 0.0001f){
			panicDir_ = Vector3::Normalized(direction); //舌の射出方向を逃走ベクトルとしてコピー
		} else{
			panicDir_ = { 0.0f, 0.0f, 1.0f };
		}
		panicTimer_ = 0.0f;

		// パニック演出：発光させる
		object_->SetColor({2.0f, 0.2f, 0.6f, 1.0f});
	}
}

void SentinelEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (!object_)
		return;

	Vector3 previousPosition = position_;
	Vector3 toPlayer = playerPos - position_;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	switch (state_) {
	case State::Idle:
		object_->SetColor({0.0f, 1.0f, 0.5f, 1.0f});
		if (player_ && !player_->IsMimicking()) {
			if (distXZ < detectRange_)
				state_ = State::Fleeing;
		}
		floatTimer_ += deltaTime;
		velocity_ = {0, 0, 0};
		position_.y = homePosition_.y + std::sin(floatTimer_ * 3.0f) * 0.3f;
		break;

	case State::Fleeing:
		object_->SetColor({1.0f, 0.5f, 0.0f, 1.0f});
		if (player_ && player_->IsMimicking()) {
			state_ = State::Returning;
		} else if (distXZ > 0.1f) {
			Vector3 fleeDir = Vector3::Normalized({-toPlayer.x, 0.0f, -toPlayer.z});
			velocity_ = fleeDir * (fleeSpeed_ / 60.0f);
			velocity_.y = 0.05f;
		}
		if (distXZ > loseRange_)
			state_ = State::Returning;
		break;

	case State::Returning:
		object_->SetColor({0.3f, 0.3f, 0.8f, 1.0f});
		{
			Vector3 toHome = homePosition_ - position_;
			if (toHome.Length() > 0.2f) {
				velocity_ = Vector3::Normalized(toHome) * (normalSpeed_ / 60.0f);
			} else {
				state_ = State::Idle;
			}
		}
		break;

	case State::Panicking:
		// --- 【スリングショット強化：ロケット推進】 ---
		{
			// 指数関数的なブースト（刺さった瞬間が一番速い）
			float startBoost = 2.8f;
			float decay = 3.5f;
			float currentBoost = 1.0f + (startBoost * std::exp(-panicTimer_ * decay));

			// パニック移動速度を適用
			velocity_ = panicDir_ * (panicSpeed_ * currentBoost / 60.0f);
		}

		panicTimer_ += deltaTime;

		// 【調整】走る時間を短縮（1.5f -> 0.8f）
		// ステージが狭いため、これくらいが丁度良い
		if (panicTimer_ > 0.8f) {
			isDead_ = true;
		}
		break;
	}

	Vector3 totalMove = velocity_;

	// 1ステップの最大移動量
	const float kMaxStep = 0.25f;

	float maxAbs =
		(std::max)(
			std::fabs(totalMove.x),
			(std::max)(std::fabs(totalMove.y), std::fabs(totalMove.z)));

	int stepCount = (std::max)(1, static_cast<int>(std::ceil(maxAbs / kMaxStep)));
	Vector3 stepMove = totalMove / static_cast<float>(stepCount);

	for(int i = 0; i < stepCount; ++i){
		Vector3 previousPosition = position_;
		position_ += stepMove;

		ResolveBlockCollisions3D(previousPosition);
		ResolveCylinderCollision();
	}

	// 向きは状態に応じて設定
	if (state_ == State::Returning) {
		Vector3 toHome = homePosition_ - position_;
		float yaw = std::atan2(toHome.x, toHome.z);
		object_->SetRotate({0.0f, yaw, 0.0f});
	} else {
		float yaw = std::atan2(toPlayer.x, toPlayer.z);
		object_->SetRotate({0.0f, yaw, 0.0f});
	}
	object_->SetTranslate(position_);
	float yaw = std::atan2(toPlayer.x, toPlayer.z);
	object_->SetRotate({0.0f, yaw, 0.0f});
	object_->SetTranslate(position_);
	object_->Update();
}

void SentinelEnemy::Draw() {
	if (object_)
		object_->Draw();
}

// SentinelEnemy.cpp

void SentinelEnemy::ResolveBlockCollisions3D(const Vector3& previousPosition){
	if(!blockColliders_){
		return;
	}

	auto intersectsAnyBlock = [&](const Vector3& testPos) -> bool{
		const CollisionUtility::OBB testObb = GetOBB(testPos, kCollisionRadius_);
		for(const auto& block : *blockColliders_){
			if(CollisionUtility::IntersectOBB_OBB(testObb, block)){
				return true;
			}
		}
		return false;
		};

	Vector3 resolved = previousPosition;

	// X 軸
	resolved.x = position_.x;
	if(intersectsAnyBlock(resolved)){
		resolved.x = previousPosition.x;
		velocity_.x = 0.0f;
	}

	// Y 軸
	resolved.y = position_.y;
	if(intersectsAnyBlock(resolved)){
		resolved.y = previousPosition.y;
		velocity_.y = 0.0f;
	}

	// Z 軸
	resolved.z = position_.z;
	if(intersectsAnyBlock(resolved)){
		resolved.z = previousPosition.z;
		velocity_.z = 0.0f;
	}

	position_ = resolved;
}

void SentinelEnemy::ResolveCylinderCollision(){
	if(!keepInsideCylinder_){
		return;
	}

	const CollisionUtility::Cylinder& cylinder = *keepInsideCylinder_;
	const float kEpsilon = 0.0001f;

	// Y 方向も汎用的に閉じ込める
	const float minY = cylinder.center.y - cylinder.halfHeight + kCollisionRadius_;
	const float maxY = cylinder.center.y + cylinder.halfHeight - kCollisionRadius_;

	if(position_.y < minY){
		position_.y = minY;
		if(velocity_.y < 0.0f){
			velocity_.y = 0.0f;
		}
	} else if(position_.y > maxY){
		position_.y = maxY;
		if(velocity_.y > 0.0f){
			velocity_.y = 0.0f;
		}
	}

	// XZ 平面で円柱の内側へ押し戻す
	const float dx = position_.x - cylinder.center.x;
	const float dz = position_.z - cylinder.center.z;
	const float distSq = dx * dx + dz * dz;

	const float limitRadius = (std::max)(0.0f, cylinder.radius - kCollisionRadius_);
	const float limitRadiusSq = limitRadius * limitRadius;

	if(distSq > limitRadiusSq){
		const float oldX = position_.x;
		const float oldZ = position_.z;

		const float dist = std::sqrt(distSq);
		if(dist > kEpsilon){
			const float invDist = 1.0f / dist;
			position_.x = cylinder.center.x + dx * invDist * limitRadius;
			position_.z = cylinder.center.z + dz * invDist * limitRadius;
		} else{
			position_.x = cylinder.center.x + limitRadius;
			position_.z = cylinder.center.z;
		}

		if(std::fabs(position_.x - oldX) > kEpsilon){
			velocity_.x = 0.0f;
		}
		if(std::fabs(position_.z - oldZ) > kEpsilon){
			velocity_.z = 0.0f;
		}
	}
}