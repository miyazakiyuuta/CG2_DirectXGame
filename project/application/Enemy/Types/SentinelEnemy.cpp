#include "SentinelEnemy.h"
#include "../../../engine/3d/Object3d.h"
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
	object_->SetColor({0.0f, 1.0f, 0.5f, 1.0f}); // 緑

	// 浮遊タイプのため重力はほぼ無視するが、壁判定のために物理ロジックは通す
	gravity_ = -0.01f;
	groundY_ = -100.0f;
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
		if (distXZ < detectRange_)
			state_ = State::Fleeing;
		floatTimer_ += deltaTime;
		velocity_.x = 0;
		velocity_.z = 0;
		position_.y = homePosition_.y + std::sin(floatTimer_ * 3.0f) * 0.3f;
		break;

	case State::Fleeing:
		object_->SetColor({1.0f, 0.5f, 0.0f, 1.0f}); // オレンジ
		if (distXZ > 0.1f) {
			Vector3 fleeDir = {-toPlayer.x / distXZ, 0.0f, -toPlayer.z / distXZ};
			position_.x += fleeDir.x * (fleeSpeed_ / 60.0f);
			position_.z += fleeDir.z * (fleeSpeed_ / 60.0f);
			position_.y = std::min(position_.y + 0.05f, homePosition_.y + 5.0f);
		}
		if (distXZ > loseRange_)
			state_ = State::Returning;
		break;

	case State::Returning:
		object_->SetColor({0.3f, 0.3f, 0.8f, 1.0f}); // 青
		Vector3 toHome = homePosition_ - position_;
		float distToHome = toHome.Length();
		if (distToHome > 0.2f) {
			Vector3 dir = Vector3::Normalized(toHome);
			position_.x += dir.x * (normalSpeed_ / 60.0f);
			position_.y += dir.y * (normalSpeed_ / 60.0f);
			position_.z += dir.z * (normalSpeed_ / 60.0f);
		} else {
			state_ = State::Idle;
		}
		if (distXZ < detectRange_)
			state_ = State::Fleeing;
		break;
	}

	// 衝突解決
	ResolveHorizontalCollisions(previousPosition);
	ResolveVerticalCollisions();

	object_->SetTranslate(position_);
	object_->Update();
}

void SentinelEnemy::Draw() {
	if (object_)
		object_->Draw();
}