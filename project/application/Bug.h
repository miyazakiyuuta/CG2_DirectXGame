#pragma once
#include "3d/Object3d.h"
#include "math/Vector3.h"
#include <memory>

class Camera;
class IBugBehavior;

class Bug {
public:
	Bug();
	virtual ~Bug();

	void Initialize(Camera* camera);
	void Update();
	void Draw();

	// 挙動制御用のインターフェース
	Vector3 GetPosition() const { return position_; }
	void ApplySteeringForce(const Vector3& desiredVelocity);
	void SetPosition(const Vector3& pos) { position_ = pos; }

	Vector3 GetRandomPositionInRange() const;
	void LookAt(const Vector3& direction);

private:
	void ChangeRandomBehavior();

	std::unique_ptr<Object3d> object_ = nullptr;
	std::unique_ptr<IBugBehavior> behavior_ = nullptr;

	Vector3 position_ = {0.0f, 2.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f}; // 慣性のための速度ベクトル

	float currentYaw_ = 0.0f; // 現在の回転（滑らかにするため）
	float jitterTimer_ = 0.0f;

	float moveAreaRadius_ = 5.0f;
	float moveAreaHeightMin_ = 1.5f;
	float moveAreaHeightMax_ = 2.5f;
};