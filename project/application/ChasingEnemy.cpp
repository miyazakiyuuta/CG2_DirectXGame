#include "ChasingEnemy.h"
#include "3d/Object3d.h"
#include <algorithm>
#include <cmath>

ChasingEnemy::ChasingEnemy() = default;
ChasingEnemy::~ChasingEnemy() = default;

void ChasingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("Cube.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scale = 1.0f;
	object_->SetScale({scale, scale, scale});
	object_->SetColor({1.0f, 0.3f, 0.3f, 1.0f}); // 赤

	groundY_ = scale;
}

void ChasingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (!object_)
		return;

	Vector3 toPlayer = playerPos - position_;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
	toPlayer.y = 0;

	// 追尾移動
	if (distXZ > 1.0f) {
		Vector3 moveDir = Vector3::Normalized(toPlayer);
		position_.x += moveDir.x * (5.0f / 60.0f); // Player の moveSpeed_ に合わせる
		position_.z += moveDir.z * (5.0f / 60.0f);
	}

	// --- プレイヤーの高さに合わせたジャンプAI ---
	if (distXZ < jumpRange_ && playerPos.y > position_.y + 0.5f && position_.y <= groundY_ + 0.01f && velocity_.y <= 0.0f) {
		float heightDiff = playerPos.y - position_.y;
		const float maxJumpHeight = 15.0f;
		float targetH = (std::min)(heightDiff, maxJumpHeight);

		// 重力が弱くなったので、計算式を調整 (v = sqrt(2gh))
		// 1.4f は「確実に飛び乗る」ための係数
		float gravityPerFrame = std::abs(gravity_ * deltaTime);
		float requiredV = std::sqrt(2.0f * gravityPerFrame * targetH * 1.4f);

		// ジャンプ力を制限 (Player の jumpPowers 0.55f ~ 1.30f 相当)
		velocity_.y = std::clamp(requiredV, 0.55f, 1.4f);
	}

	ApplyGravity(deltaTime);

	object_->SetTranslate(position_);
	object_->Update();
}

void ChasingEnemy::Draw() {
	if (object_)
		object_->Draw();
}