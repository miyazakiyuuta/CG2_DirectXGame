#include "ShootingEnemy.h"
#include "3d/Object3d.h"

ShootingEnemy::ShootingEnemy() = default;
ShootingEnemy::~ShootingEnemy() = default;

void ShootingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("Cube.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scaleY = 1.2f; // 少し背を高く
	object_->SetScale({0.8f, scaleY, 0.8f});
	object_->SetColor({0.3f, 0.3f, 1.0f, 1.0f}); // 青

	// 【ここでいじる】埋まり防止の接地高さ
	groundY_ = scaleY;
}

void ShootingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	shotTimer_ += deltaTime;
	if (shotTimer_ >= kShotInterval) {
		Shoot(playerPos);
		shotTimer_ = 0.0f;
	}

	for (auto it = bullets_.begin(); it != bullets_.end();) {
		(*it)->Update(deltaTime);
		if ((*it)->IsDead())
			it = bullets_.erase(it);
		else
			++it;
	}

	ApplyGravity(deltaTime);

	if (object_) {
		object_->SetTranslate(position_);
		object_->Update();
	}
}

void ShootingEnemy::Shoot(const Vector3& target) {
	auto b = std::make_unique<EnemyBullet>();
	b->Initialize(common_, camera_, position_, target);
	bullets_.push_back(std::move(b));
}

void ShootingEnemy::Draw() {
	if (object_)
		object_->Draw();
	for (auto& b : bullets_)
		b->Draw();
}