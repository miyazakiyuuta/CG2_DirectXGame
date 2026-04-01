#pragma once
#include "math/Vector3.h"
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;
class Camera;

/// <summary>
/// 敵の基底クラス。Player.h の重力設定に合わせて調整。
/// </summary>
class BaseEnemy {
public:
	BaseEnemy();
	virtual ~BaseEnemy();

	virtual void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) = 0;
	virtual void Update(float deltaTime, const Vector3& playerPos) = 0;
	virtual void Draw() = 0;

	const Vector3& GetPosition() const { return position_; }
	bool IsDead() const { return isDead_; }
	void Kill() { isDead_ = true; }

protected:
	// 重力計算：接地高さ(groundY_)で止まるように調整
	void ApplyGravity(float deltaTime);

protected:
	std::unique_ptr<Object3d> object_ = nullptr;
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	bool isDead_ = false;

	// --- 物理パラメータ ---
	// 修正：Player.h の -0.02f/frame に合わせるため、秒間重力を -1.2f に設定
	// (-1.2f * 1/60 = -0.02f)
	float gravity_ = -1.2f;
	float groundY_ = 0.0f;
};