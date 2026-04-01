#include "SentinelEnemy.h"
#include "3d/Object3d.h"
#include <algorithm>
#include <cmath>

SentinelEnemy::SentinelEnemy() : homePosition_({0, 0, 0}), state_(State::Idle), floatTimer_(0.0f), normalSpeed_(3.0f), fleeSpeed_(12.0f), detectRange_(20.0f), loseRange_(35.0f) {}

SentinelEnemy::~SentinelEnemy() = default;

void SentinelEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("Cube.obj");
	object_->SetCamera(camera);

	position_ = pos;
	homePosition_ = pos;

	object_->SetScale({0.7f, 0.7f, 0.7f});
	object_->SetColor({0.0f, 1.0f, 0.5f, 1.0f}); // 初期はエメラルドグリーン

	// 浮遊タイプのため重力を無効化
	gravity_ = 0.0f;
	groundY_ = -100.0f;
}

void SentinelEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (!object_)
		return;

	// プレイヤーとの水平距離を計算
	Vector3 toPlayer = playerPos - position_;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	switch (state_) {
	case State::Idle:
		object_->SetColor({0.0f, 1.0f, 0.5f, 1.0f}); // 緑
		if (distXZ < detectRange_) {
			state_ = State::Fleeing;
		}
		// ふわふわした待機移動
		floatTimer_ += deltaTime;
		position_.y = homePosition_.y + std::sin(floatTimer_ * 3.0f) * 0.3f;
		break;

	case State::Fleeing:
		object_->SetColor({1.0f, 0.5f, 0.0f, 1.0f}); // オレンジ（警戒）

		// プレイヤーから反対方向へ逃げる
		if (distXZ > 0.1f) {
			Vector3 fleeDir = {-toPlayer.x / distXZ, 0.0f, -toPlayer.z / distXZ};
			position_.x += fleeDir.x * fleeSpeed_ * deltaTime;
			position_.z += fleeDir.z * fleeSpeed_ * deltaTime;

			// 逃げる際は高度を上げる
			position_.y = std::min(position_.y + 2.0f * deltaTime, homePosition_.y + 5.0f);
		}

		// 一定以上離れたら諦めて戻る
		if (distXZ > loseRange_) {
			state_ = State::Returning;
		}
		break;

	case State::Returning:
		object_->SetColor({0.3f, 0.3f, 0.8f, 1.0f}); // 青（帰還）

		Vector3 toHome = homePosition_ - position_;
		float distToHome = std::sqrt(toHome.x * toHome.x + toHome.y * toHome.y + toHome.z * toHome.z);

		if (distToHome > 0.2f) {
			Vector3 returnDir = {toHome.x / distToHome, toHome.y / distToHome, toHome.z / distToHome};
			position_.x += returnDir.x * normalSpeed_ * deltaTime;
			position_.y += returnDir.y * normalSpeed_ * deltaTime;
			position_.z += returnDir.z * normalSpeed_ * deltaTime;
		} else {
			position_ = homePosition_;
			state_ = State::Idle;
		}

		// 帰還中にプレイヤーが近づいたら再度逃走
		if (distXZ < detectRange_) {
			state_ = State::Fleeing;
		}
		break;
	}

	object_->SetTranslate(position_);
	object_->Update();
}

void SentinelEnemy::Draw() {
	if (object_)
		object_->Draw();
}