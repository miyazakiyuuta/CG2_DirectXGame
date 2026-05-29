#include "EnemyBullet.h"
#include "3d/Camera.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include <cmath>
#include <numbers>
EnemyBullet::EnemyBullet() = default;
EnemyBullet::~EnemyBullet() = default;

void EnemyBullet::Initialize(
	Object3dCommon* common,
	Camera* camera,
	const Vector3& pos,
	const Vector3& target,
	int damage
) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("Bullet.obj");
	object_->SetCamera(camera);
	position_ = pos;
	damage_ = damage < 0 ? 0 : damage;
	object_->SetScale({0.3f, 0.3f, 0.3f});
	object_->SetColor({244.0f / 255.0f, 234.0f / 255.0f, 230.0f / 255.0f, 1.0f}); // #F4EAE6

	Vector3 dir = target - pos;
	if (dir.Length() > 0) {
		velocity_ = Vector3::Normalized(dir) * speed_ * (1.0f / 60.0f);
		
		Vector3 vDir = Vector3::Normalized(velocity_);
		// モデルが-X軸方向を向いているため、Y軸回転とZ軸回転を組み合わせて完全な3D方向（縦・横）に向かせる
		// ジンバルロックを避けるための特別なオイラー角計算
		float yaw = std::asin(vDir.z);
		float roll = std::atan2(-vDir.y, -vDir.x);
		
		// X軸(pitch)は0にし、Y軸(yaw)で左右、Z軸(roll)で上下の傾きを作る
		object_->SetRotate({0.0f, yaw, roll});
	}

	object_->SetTranslate(position_);
	object_->Update();
}

void EnemyBullet::Update(float deltaTime) {
	position_ += velocity_;
	deathTimer_ -= deltaTime;
	if (deathTimer_ <= 0.0f)
		isDead_ = true;
	if (object_) {
		object_->SetTranslate(position_);
		object_->Update();
	}
}

void EnemyBullet::Draw() {
	if (object_)
		object_->Draw();
}