#pragma once

#define NOMINMAX
#include <memory>
#include <string>
#include "3d/Object3d.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include "Tongue.h"

class Camera;
class Object3dCommon;
class CameraController;

class Player{

	enum class MovementState{
		Root,         // 接地（移動・チャージ可）
		Jumping,      // 空中
		WallClinging  // 壁張り付き
	};


public:
	Player() = default;
	~Player();

	void Initialize(
		Object3dCommon* object3dCommon,
		Camera* camera,
		const std::string& modelName,
		const Vector3& startPosition = { 0.0f, 0.0f, 0.0f }
	);

	void Update(float cameraYaw);
	void Draw();

	void SetCamera(Camera* camera);
	void SetPosition(const Vector3& position);
	Vector3 GetPosition() const;
	Vector3 GetVelocity() const{ return velocity_; }

	void SetGroundHeight(float groundHeight){ groundHeight_ = groundHeight; }
	bool IsOnGround() const{ return isOnGround_; }

	// チャージジャンプ
	float GetJumpChargeRate() const;
	int GetCurrentVisibleChargeLevel() const;
	int GetChargeStock() const{ return chargeStock_; }
	void AddChargeStock(int amount);
	void SetChargeStock(int stock);
	int GetCurrentChargePhase() const;
	float GetCurrentChargePhaseRate() const;
	bool IsChargeAtMaxPhase() const;

	float GetYaw() const;
	Tongue* GetTongue() const{ return tongue_.get(); }

	void DrawImGui();

private:
	void MoveHorizontal(float cameraYaw);
	void UpdateJumpCharge();
	void ApplyGravity();

	int GetCurrentChargeLevel() const;
	int GetAllowedChargeLevel() const;
	void ExecuteChargedJump(int chargeLevel);
	void CancelJumpCharge();

	void UpdateWallClinging(float cameraYaw);
	void TransitionTo(MovementState nextState);
	const char* GetMovementStateName() const;
private:
	std::unique_ptr<Object3d> object_ = nullptr;
	Camera* camera_ = nullptr;
	Input* input_ = nullptr;

	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	Vector3 lastMove_ = { 0.0f, 0.0f, 0.0f };

	float moveSpeed_ = 5.0f / 60.0f;
	float gravity_ = -0.02f;
	float groundHeight_ = 0.0f;
	float resetHeight_ = 3.0f;
	bool isOnGround_ = false;

	// モデル正面ズレ補正
	float modelYawOffset_ = 0.0f;

	// チャージジャンプ
	float jumpPowers_[4] = { 0.55f, 0.80f, 1.05f, 1.30f };
	float chargeThresholds_[3] = { 40.0f, 80.0f, 120.0f };
	float chargeCancelHoldLimit_ = 240.0f;

	bool isChargingJump_ = false;
	bool isJumpChargeCanceled_ = false;
	float chargeTimer_ = 0.0f;
	float chargeMaxHoldTimer_ = 0.0f;

	int chargeStock_ = 3;
	static constexpr int kMaxChargeLevel_ = 3;

	std::unique_ptr<Tongue> tongue_ = nullptr;

	MovementState moveState_ = MovementState::Root;

	float wallClingGauge_ = 100.0f;       // 壁張り付きゲージ
	float maxWallClingGauge_ = 100.0f;    // 最大値
	float wallClingConsumption_ = 0.5f;   // 消費速度
	float wallMoveSpeed_ = 0.05f;         // 壁移動速度

	Vector3 wallRightVec_ = { 1.0f, 0.0f, 0.0f }; // 壁上での左右移動軸
	// 上下は Vector3(0, 1, 0) で固定
};