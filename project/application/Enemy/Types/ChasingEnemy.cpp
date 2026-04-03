#include "ChasingEnemy.h"
#include "../../../engine/3d/Object3d.h"
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

	// 【事実】プレイヤーの colliderHalfSize_ = {1.0, 1.0, 1.0} に合わせる。
	// これにより、プレイヤーと同じ接地感（少し浮いているような安定感）を再現します。
	object_->SetScale({1.0f, 1.0f, 1.0f});
	object_->SetColor({1.0f, 0.3f, 0.3f, 1.0f});

	// 最低地面の基準を 0.0f に設定。
	groundY_ = 0.0f;
	gravity_ = -0.025f;

	// 出現時に即座に埋まらないよう、高めに配置。
	position_.y = (std::max)(position_.y, 2.5f);
}

void ChasingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (!object_)
		return;

	Vector3 prevPos = position_;
	Vector3 toPlayer = playerPos - position_;
	toPlayer.y = 0;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	// 水平移動速度を 60FPS 基準に補正。
	float actualSpeed = (speed_ > 1.0f) ? (speed_ / 60.0f) : speed_;

	if (distXZ > 1.0f) {
		Vector3 moveDir = Vector3::Normalized(toPlayer);
		position_.x += moveDir.x * actualSpeed;
		position_.z += moveDir.z * actualSpeed;
	}

	// 1. 水平衝突解決 (BaseEnemy.cpp の 1.0f 半径を使用)
	ResolveHorizontalCollisions(prevPos);

	// 2. ジャンプAI
	if (distXZ < jumpRange_ && playerPos.y > position_.y + 0.5f && isOnGround_) {
		float heightDiff = playerPos.y - position_.y;
		float requiredV = std::sqrt(2.0f * std::abs(gravity_) * heightDiff * 1.4f);
		velocity_.y = (std::clamp)(requiredV, 0.6f, 1.4f);
		isOnGround_ = false;
	}

	// 3. 垂直移動
	velocity_.y += gravity_;
	position_.y += velocity_.y;

	// 4. 垂直衝突解決 (これでブロックの上に乗れるようになります)
	ResolveVerticalCollisions();

	object_->SetTranslate(position_);
	object_->Update();
}

void ChasingEnemy::Draw() {
	if (object_)
		object_->Draw();
}