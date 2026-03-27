#pragma once
#include "math/Vector3.h"
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;
class Camera;

/// <summary>
/// 敵の基底クラス。コンストラクタとデストラクタをcppに書くことで不完全型エラーを防止。
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
	// 重力計算：groundY_ でピタッと止まる
	void ApplyGravity(float deltaTime);

protected:
	std::unique_ptr<Object3d> object_ = nullptr;
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	bool isDead_ = false;

	// --- 物理パラメータ (ここで重力の強さを変えられます) ---
	float gravity_ = -32.0f;
	// --- 接地高さ (これより下にいかない。Initializeで設定します) ---
	float groundY_ = 0.0f;
};