#include "ShootingEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include "../../Player.h"

ShootingEnemy::ShootingEnemy() = default;
ShootingEnemy::~ShootingEnemy() = default;

void ShootingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	LoadModel("Cube.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scaleY = 1.2f; // 少し背を高く
    object_->SetScale({0.8f, scaleY, 0.8f});
	SetColor({0.3f, 0.3f, 1.0f, 1.0f}); // 青

	groundY_ = scaleY;
}

void ShootingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	// 弾の更新（死亡後も飛ばし続ける）
	for (auto it = bullets_.begin(); it != bullets_.end();) {
		(*it)->Update(deltaTime);
		if ((*it)->IsDead())
			it = bullets_.erase(it);
		else
			++it;
	}

	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		return;
	}

	// 1. 水平移動前の座標を保持
	Vector3 previousPosition = position_;

	// 射撃タイマー
	if (!player_ || !player_->IsMimicking()) {
		shotTimer_ += deltaTime;
		if (shotTimer_ >= kShotInterval) {
			Shoot(playerPos);
			shotTimer_ = 0.0f;
		}
	}

	// 2. 水平移動の解決（移動がなくても壁判定のために呼ぶ）
	ResolveHorizontalCollisions(previousPosition);

	// 3. 垂直移動の計算
	velocity_.y += gravity_; // 重力加算
	position_.y += velocity_.y;

	// 4. 垂直衝突解決（接地判定）
	ResolveVerticalCollisions();

	if (object_) {
		object_->SetTranslate(position_);
	}
}

void ShootingEnemy::Shoot(const Vector3& target) {
	auto b = std::make_unique<EnemyBullet>();
	b->Initialize(
		common_,
		camera_,
		position_,
		target,
		GetProjectileDamage()
	);
	bullets_.push_back(std::move(b));
}

void ShootingEnemy::Draw() {
	object_->Update();
	if (isDead_) {
		DrawDeathAnimation();
	} else if (object_) {
		object_->Draw();
	}

	for (auto& b : bullets_)
		b->Draw();
}