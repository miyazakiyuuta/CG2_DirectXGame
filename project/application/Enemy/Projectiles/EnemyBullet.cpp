#include "EnemyBullet.h"
#include "3d/Camera.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

EnemyBullet::EnemyBullet() = default;
EnemyBullet::~EnemyBullet() = default;

void EnemyBullet::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos, const Vector3& target) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("sphere.obj");
	object_->SetCamera(camera);
	position_ = pos;
	object_->SetScale({0.3f, 0.3f, 0.3f});
	object_->SetColor({1.0f, 0.8f, 0.0f, 1.0f}); // 黄色

	Vector3 dir = target - pos;
	if (dir.Length() > 0)
		velocity_ = Vector3::Normalized(dir) * speed_ * (1.0f / 60.0f);
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