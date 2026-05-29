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
	const std::list<std::unique_ptr<EnemyBullet>>& GetBullets() const { return bullets_; }

private:
	// プレイヤーへの視線がブロックで遮蔽されているかチェック
	bool IsLineOfSightBlocked(const Vector3& target) const;

	void Shoot(const Vector3& target);

	std::list<std::unique_ptr<EnemyBullet>> bullets_;
	float shotTimer_ = 0.0f;
	const float kShotInterval = 2.5f;

	// --- 感知パラメータ ---
	float detectionRange_ = 30.0f;       // 水平方向の感知距離
	float detectionHeightRange_ = 15.0f;  // 垂直方向の感知距離
	bool isPlayerDetected_ = false;       // 現在プレイヤーを感知しているか
};