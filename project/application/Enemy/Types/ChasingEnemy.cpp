#include "ChasingEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include "../../Player.h"
#include <algorithm>
#include <cmath>

ChasingEnemy::ChasingEnemy() = default;
ChasingEnemy::~ChasingEnemy() = default;

void ChasingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	LoadModel("Cube.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scale = 1.0f;
    object_->SetScale({scale, scale, scale});
	SetColor({1.0f, 0.3f, 0.3f, 1.0f}); // 赤

	// 最低地面の基準を 0.0f に設定。
	groundY_ = 0.0f;
	gravity_ = -0.025f;

	// 出現時に即座に埋まらないよう、高めに配置。
	position_.y = (std::max)(position_.y, 2.5f);
}

void ChasingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		return;
	}

	if (!object_)
		return;

	Vector3 prevPos = position_;
	Vector3 toPlayer = playerPos - position_;
	toPlayer.y = 0;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	// 水平移動速度を 60FPS 基準に補正。
	float actualSpeed = (speed_ > 1.0f) ? (speed_ / 60.0f) : speed_;

	if (distXZ > 1.0f && (!player_ || !player_->IsMimicking())) {
		Vector3 moveDir = Vector3::Normalized(toPlayer);
		position_.x += moveDir.x * actualSpeed;
		position_.z += moveDir.z * actualSpeed;
	}

	// 1. 水平衝突解決 (BaseEnemy.cpp の 1.0f 半径を使用)
	ResolveHorizontalCollisions(prevPos);

	// 2. ジャンプAI
	if (distXZ < jumpRange_ && playerPos.y > position_.y + 0.5f && isOnGround_ && (!player_ || !player_->IsMimicking())) {
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
}

void ChasingEnemy::Draw() {
	object_->Update();
	if (isDead_) {
		DrawDeathAnimation();
		return;
	}

	if (object_)
		object_->Draw();
}