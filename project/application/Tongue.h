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

	void Shot();
	void Reset();

	bool IsBusy() const{ return state_ != State::Idle; }
	State GetState() const{ return state_; }
	Vector3 GetPosition() const{ return worldPosition_; }

	// 舌先の虫判定用
	bool CanHitBug() const{ return state_ == State::Extending; }
	CollisionUtility::Sphere GetHitSphere() const;

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
	Vector3 localOffset_ = { 0.0f, 0.0f, 1.0f };

	Vector3 worldPosition_ = {};
	Vector3 shotDirection_ = {};
	Vector3 shotStartPosition_ = {};

	float speed_ = 35.0f;
	float maxDistance_ = 10.0f;
	float returnSpeed_ = 45.0f;

	float currentDistance_ = 0.0f;

	// 舌先球の半径
	float hitRadius_ = 0.35f;
};