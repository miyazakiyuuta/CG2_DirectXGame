#pragma once

#include <memory>
#include <string>
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"

class Camera;
class Object3d;
class Object3dCommon;
class Player;

class Tongue{
public:
	enum class State{
		Idle,
		Extending,
		Hooked,
		Returning,
	};

	Tongue() = default;
	~Tongue() = default;

	void Initialize(
		Object3dCommon* object3dCommon,
		Camera* camera,
		Player* owner,
		const std::string& modelName
	);

	void Update(float deltaTime);
	void Draw();

	void Shot(const Vector3& direction);
	void ShotSweep(const Vector3& direction, float halfAngleDeg, float duration);
	void Reset();
	void StartReturn();
	void SetHooked(const Vector3& worldPos);

	bool IsBusy() const{ return state_ != State::Idle; }
	bool IsHooked() const{ return state_ == State::Hooked; }
	State GetState() const{ return state_; }

	Vector3 GetPosition() const{ return worldPosition_; }
	Vector3 GetPrevPosition() const{ return prevWorldPosition_; }

	void SetAlpha(float alpha){ currentAlpha_ = alpha; }
	float GetAlpha() const{ return currentAlpha_; }

	// 舌先の虫判定用
	bool CanHitBug() const{ return state_ == State::Extending; }
	CollisionUtility::Sphere GetHitSphere() const;

	Vector3 GetMouthWorldPositionPublic() const{ return GetMouthWorldPosition(); }

private:
	void UpdateIdle();
	void UpdateExtending(float deltaTime);
	void UpdateReturning(float deltaTime);
	Vector3 GetMouthWorldPosition() const;

private:
	Player* owner_ = nullptr;
	Camera* camera_ = nullptr;
	std::unique_ptr<Object3d> object_ = nullptr;

	State state_ = State::Idle;

	// プレイヤー口元からのローカルオフセット
	Vector3 localOffset_ = { 0.0f, 0.0f, 0.4f };

	Vector3 worldPosition_ = {};
	Vector3 prevWorldPosition_ = {};
	Vector3 hookPosition_ = {};
	Vector3 shotDirection_ = {};
	Vector3 shotStartPosition_ = {};

	float speed_ = 65.0f;
	float maxDistance_ = 30.0f;
	float returnSpeed_ = 45.0f;

	// スイープ用のオリジナル速度
	float originalSpeed_ = 35.0f;
	float originalReturnSpeed_ = 45.0f;

	// スイープ用の速度倍率（これを掛けると速くなり、スイープ中の見た目がより顕著になる）
	float launchSpeedMultiplier_ = 2.0f; 
	float returnSpeedMultiplier_ = 2.0f; 

	float currentDistance_ = 0.0f;

	// 舌先球の半径
	float hitRadius_ = 0.3f;

	float currentAlpha_ = 1.0f;

	// スイープ状態の管理
	bool sweeping_ = false;
	float sweepHalfAngleRad_ = 0.0f;
	float sweepDuration_ = 0.0f;
	float sweepTimer_ = 0.0f;
	Vector3 baseDirection_ = {0.0f, 0.0f, 1.0f};
    // スイープ開始時の発射方向（fanの左端）
	Vector3 sweepLaunchDirection_ = {0.0f, 0.0f, 1.0f};
    // スイープ中、舌先が当たり判定から少し先まで伸びるようにする追加距離（これにより、スイープの見た目と当たり判定のズレを減らす）
	float extraExtendDistance_ = 4.0f;
};