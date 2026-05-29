#pragma once
#include "Enemy/Core/BaseEnemy.h"

class ChasingEnemy : public BaseEnemy {
public:
	ChasingEnemy();
	~ChasingEnemy() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

private:
	// プレイヤーに最も近い、ジャンプで到達可能な足場を探す
	// 見つかった場合は targetBlockTop にブロック上面Y座標を返す
	bool FindBestPlatformToward(const Vector3& playerPos, float& targetBlockTop) const;

	float speed_ = 4.0f;       // 移動速度
	float jumpRange_ = 15.0f;  // ジャンプ判定の水平距離

	// --- 感知パラメータ ---
	float detectionRange_ = 25.0f;       // 水平方向の感知距離
	float detectionHeightRange_ = 20.0f;  // 垂直方向の感知距離

	// --- ジャンプパラメータ ---
	float maxJumpVelocity_ = 0.8f;  // ジャンプ速度の上限
};