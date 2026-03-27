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

	// 【ここでいじる】埋まるのを防ぐための高さ設定
	// モデルの中心がどの高さにあれば「地面の上」に見えるかを決めます。
	// Cube.obj（サイズ2.0）なら scale を入れるとちょうど接地します。
	groundY_ = scale;
}

void ChasingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (!object_)
		return;

	Vector3 toPlayer = playerPos - position_;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
	toPlayer.y = 0;

	// 追尾
	if (distXZ > 1.0f) {
		Vector3 moveDir = Vector3::Normalized(toPlayer);
		position_.x += moveDir.x * speed_ * deltaTime;
		position_.z += moveDir.z * speed_ * deltaTime;
	}

	// --- プレイヤーの高さに合わせて制限付きジャンプ ---
	// 条件: 近くにいる ＆ プレイヤーが自分より上にいる ＆ 自分が地面にいる
	if (distXZ < jumpRange_ && playerPos.y > position_.y + 0.5f && position_.y <= groundY_ + 0.01f && velocity_.y <= 0.0f) {
		float heightDiff = playerPos.y - position_.y;

		// 【ここでいじる】これ以上高くは飛ばない限界値
		const float maxJumpHeight = 120.0f;
		float targetH = (std::min)(heightDiff, maxJumpHeight);

		// 物理公式 v = sqrt(2gh) を使用（1.3f は余裕を持たせるための補正係数）
		float gravityMag = std::abs(gravity_ * deltaTime);
		float requiredV = std::sqrt(2.0f * gravityMag * targetH * 1.3f);

		// ジャンプ力を一定範囲に制限
		velocity_.y = std::clamp(requiredV, 0.3f, 1.3f);
	}

	ApplyGravity(deltaTime);

	object_->SetTranslate(position_);
	object_->Update();
}

void ChasingEnemy::Draw() {
	if (object_)
		object_->Draw();
}