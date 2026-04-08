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
	object_->SetModel("Cube.obj");
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
		panicDir_ = direction; // 舌の射出方向を逃走ベクトルとしてコピー
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

	position_ += velocity_;
	ResolveHorizontalCollisions(previousPosition);

	// パニック中、常に移動方向を向く（プレイヤーが飛ばされる方向のガイドになる）
	if (state_ == State::Panicking || state_ == State::Fleeing) {
		if (velocity_.Length() > 0.01f) {
			float yaw = std::atan2(velocity_.x, velocity_.z);
			object_->SetRotate({0.0f, yaw, 0.0f});
		}
	}

	object_->SetTranslate(position_);
	object_->Update();
}

void SentinelEnemy::Draw() {
	if (object_)
		object_->Draw();
}