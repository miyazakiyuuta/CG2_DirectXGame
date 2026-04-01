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
	float speed_ = 4.0f;      // 移動速度
	float jumpRange_ = 1.0f; // プレイヤーがこの距離にいたらジャンプを考慮
};