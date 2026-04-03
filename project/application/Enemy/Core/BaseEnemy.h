#pragma once
#include "../../../engine/math/Vector3.h"
#include "../../../engine/utility/CollisionUtility.h"
#include "math/Vector4.h"
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;
class Camera;

class BaseEnemy {
public:
	BaseEnemy();
	virtual ~BaseEnemy();

	virtual void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) = 0;
	virtual void Update(float deltaTime, const Vector3& playerPos) = 0;
	virtual void Draw() = 0;

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) { blockColliders_ = colliders; }

	const Vector3& GetPosition() const { return position_; }
	bool IsDead() const { return isDead_; }
	void Kill() { isDead_ = true; }

    // Color helpers: derived classes should call SetColor during Initialize
	void SetColor(const Vector4& color);
	void SetAlpha(float a);
	// Return the originally assigned alpha (stored when SetColor was called)
	float GetOriginalAlpha() const;

	// プレイヤーへの速度補正値を取得
	float GetPlayerSpeedMultiplier() const { return playerSpeedMultiplier_; }

protected:
	void ResolveHorizontalCollisions(const Vector3& previousPosition);
	void ResolveVerticalCollisions();

	void ResolveHorizontalCollisionsForPos(Vector3& pos, const Vector3& prevPos, float radius) const;
	void ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround) const;

	virtual CollisionUtility::OBB GetOBB(const Vector3& pos, float radius) const;

protected:
	std::unique_ptr<Object3d> object_ = nullptr;
    Vector4 originalColor_ = {1.0f,1.0f,1.0f,1.0f};
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	bool isDead_ = false;
	bool isOnGround_ = false;

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;

	float gravity_ = -0.04f;
	float groundY_ = 0.0f;

	// プレイヤーの移動速度倍率（1.0が通常）
	float playerSpeedMultiplier_ = 1.0f;
};