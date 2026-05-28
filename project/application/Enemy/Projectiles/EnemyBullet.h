// EnemyBullet.h
#pragma once
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"
#include <memory>

class Object3d;
class Object3dCommon;
class Camera;

class EnemyBullet {
public:
	EnemyBullet();
	~EnemyBullet();

	void Initialize(
		Object3dCommon* common,
		Camera* camera,
		const Vector3& start,
		const Vector3& target,
		int damage = 1
	);
	void Update(float deltaTime);
	void Draw();

	bool IsDead() const { return isDead_; }
	void Kill() { isDead_ = true; }

	const Vector3& GetPosition() const { return position_; }
	float GetRadius() const { return 0.3f; }

	CollisionUtility::Sphere GetHitSphere() const {
		CollisionUtility::Sphere s;
		s.center = position_;
		s.radius = GetRadius();
		return s;
	}

	int GetDamage() const { return damage_; }

private:
	std::unique_ptr<Object3d> object_ = nullptr;
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	float speed_ = 20.0f;
	float deathTimer_ = 3.0f;
	bool isDead_ = false;

	int damage_ = 1;
};