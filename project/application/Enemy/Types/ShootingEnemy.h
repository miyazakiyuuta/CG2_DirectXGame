#pragma once
#include "Enemy/Core/BaseEnemy.h"
#include "Enemy/Projectiles/EnemyBullet.h"
#include <list>

class ShootingEnemy : public BaseEnemy {
public:
	ShootingEnemy();
	~ShootingEnemy() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

private:
	void Shoot(const Vector3& target);

	Object3dCommon* common_ = nullptr;
	Camera* camera_ = nullptr;
	std::list<std::unique_ptr<EnemyBullet>> bullets_;
	float shotTimer_ = 0.0f;
	const float kShotInterval = 2.5f;
};