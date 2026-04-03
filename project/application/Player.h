#pragma once

#define NOMINMAX
#include <memory>
#include <string>
#include <vector>

#include "3d/Object3d.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include "Tongue.h"
#include "utility/CollisionUtility.h"

class Camera;
class Object3dCommon;
class CameraController;
class EnemyManager;

class Player{

	enum class MovementState{
		Root,
		Jumping,
		WallClinging,
		TonguePulling
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

	void Update();
	void Draw();
	void DrawImGui();

	void SetCamera(Camera* camera);
	void SetCameraController(CameraController* cameraController){ cameraController_ = cameraController; }

	void SetPosition(const Vector3& position);

	// Request a teleport to be applied on the next Player::Update call.
	// Using this avoids other systems overwriting the teleport by
	// ensuring the player applies it inside its own update logic.
	void SetPendingTeleport(const Vector3& position);

	Vector3 GetPosition() const;
	Vector3 GetVelocity() const{ return velocity_; }
	void SetVelocity(const Vector3& v){ velocity_ = v; }
	// Moving platform support: set per-frame platform delta applied to player
	void SetRidingPlatformDelta(const Vector3& delta);
	void ClearRidingPlatformDelta();

	void SetGroundHeight(float groundHeight){ groundHeight_ = groundHeight; }
	bool IsOnGround() const{ return isOnGround_; }

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* blockColliders){
		blockColliders_ = blockColliders;
	}

	void SetMovementLimitCylinder(const CollisionUtility::Cylinder* cylinder){
		movementLimitCylinder_ = cylinder;
	}

	CollisionUtility::OBB GetPlayerOBB(const Vector3& position) const;

	// 水分ゲージ
	float GetWaterGauge() const{ return waterGauge_; }
	float GetMaxWaterGauge() const{ return maxWaterGauge_; }
	void AddWater(float amount);
	bool ConsumeWater(float amount);

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

	// Abilities
	enum class Ability{
		Camouflage,
		Sonar,
	};

    Ability GetCurrentAbility() const { return currentAbility_; }
	void SetStage(class Stage* stage) { stage_ = stage; }

    // Mimic control
	void EndMimic(){
		if(!object_) return;
		// restore original visual
		object_->SetModel(originalModelName_);
		object_->SetScale(originalScale_);
		Vector4 c = originalColor_;
		c.w = currentAlpha_;
		object_->SetColor(c);
		isMimicking_ = false;
	}

	void UpdateTransparencyByCamera(const Vector3& cameraPosition);
	float GetCurrentAlpha() const{ return currentAlpha_; }

	void SetAlpha(float alpha){ currentAlpha_ = alpha; }

	bool TryShotTongue(const Vector3& direction);
    bool TryUseBeam(const Vector3& direction);
	void SetYawFromCamera(float cameraYaw);
	void SetEnemyManager(EnemyManager* mgr){ enemyManager_ = mgr; }

	void SetAimTargetPoint(const Vector3& point){
		aimTargetPoint_ = point;
		hasAimTargetPoint_ = true;
	}

	void ClearAimTargetPoint(){
		hasAimTargetPoint_ = false;
	}

private:
	void MoveHorizontal(float cameraYaw);
	void UpdateJumpCharge();
	void ApplyGravity();

	int GetCurrentChargeLevel() const;
	int GetAllowedChargeLevel() const;
	void ExecuteChargedJump(int chargeLevel);
	void CancelJumpCharge();

	void UpdateWallClinging(float cameraYaw);
	void UpdateTonguePulling();
	void CheckTongueBlockHook();

	void TransitionTo(MovementState nextState);
	const char* GetMovementStateName() const;

	void ResolveHorizontalCollisions(const Vector3& previousPosition);
	void ResolveVerticalCollisions(const Vector3& previousPosition);

	void SetupClingSurfaceFromHit(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& hitNormal);
	bool IsInsideCurrentClingSurface(const Vector3& position) const;
	Vector3 ClampPositionToCurrentClingSurface(const Vector3& position) const;
	void ResolveCurrentClingPenetration(Vector3& position) const;

	void ResolveWallClingBlockCollisions(Vector3& position) const;

	void ResolveMovementLimitCylinder();

	bool CanStartTongueShot() const;

private:
	std::unique_ptr<Object3d> object_ = nullptr;
	Camera* camera_ = nullptr;
	CameraController* cameraController_ = nullptr;
	Input* input_ = nullptr;

	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	Vector3 lastMove_ = { 0.0f, 0.0f, 0.0f };

	float moveSpeed_ = 20.0f / 60.0f;
	float gravity_ = -0.02f;
	float groundHeight_ = 0.0f;
	float resetHeight_ = 3.0f;
	bool isOnGround_ = false;

	// モデル正面ズレ補正
	float modelYawOffset_ = 0.0f;

	// 水分ゲージ
	float waterGauge_ = 100.0f;
	float maxWaterGauge_ = 100.0f;
	float tongueWaterCost_ = 0.0f;

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

	float wallClingGauge_ = 100.0f;
	float maxWallClingGauge_ = 100.0f;
	float wallClingConsumption_ = 0.5f;
	float wallMoveSpeed_ = 0.05f;

	Vector3 wallRightVec_ = { 1.0f, 0.0f, 0.0f };

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;
	const CollisionUtility::Cylinder* movementLimitCylinder_ = nullptr;

	Vector3 colliderHalfSize_ = { 1.0f,1.0f,1.0f };

	// Pending teleport requested by external systems (applied inside Update)
	bool pendingTeleport_ = false;
	Vector3 pendingTeleportPosition_ = { 0.0f, 0.0f, 0.0f };

	// 舌で引っ張られる処理
	Vector3 tonguePullTarget_ = { 0.0f, 0.0f, 0.0f };
	float tonguePullSpeed_ = 18.0f / 60.0f;
	float tonguePullEndDistance_ = 0.35f;
	float tongueHookSurfaceOffset_ = 0.05f;

	float currentAlpha_ = 1.0f;
	float minAlpha_ = 0.0f;

	// この距離より近づくと透け始める
	float fadeStartDistance_ = 10.0f;

	// この距離以下なら最小アルファ
	float fadeEndDistance_ = 2.5f;

	bool prevAimMode_ = false;

	Vector3 tongueHookNormal_ = { 0.0f, 0.0f, -1.0f };

	bool hasClingSurface_ = false;
	CollisionUtility::OBB clingBlockObb_ = {};
	Vector3 clingSurfaceNormal_ = { 0.0f, 0.0f, -1.0f };
	Vector3 clingSurfaceRight_ = { 1.0f, 0.0f, 0.0f };
	Vector3 clingSurfaceUp_ = { 0.0f, 1.0f, 0.0f };
	Vector3 clingSurfaceCenter_ = { 0.0f, 0.0f, 0.0f };

	float clingSurfaceHalfWidth_ = 0.0f;
	float clingSurfaceHalfHeight_ = 0.0f;

	float clingHitRightOffset_ = 0.0f;
	float clingHitUpOffset_ = 0.0f;

	float wallDetachMargin_ = 0.05f;
	float wallKeepDistance_ = 0.03f;

	bool useTonguePull_ = true;

	// Beam attack (扇状薙ぎ払い) parameters
	EnemyManager* enemyManager_ = nullptr;
	float beamCooldown_ = 60.0f; // frames
	float beamTimer_ = 0.0f;
	float beamRange_ = 8.0f;
	float beamHalfAngleDeg_ = 30.0f; // 扇の半角度
	float beamCapsuleRadius_ = 0.8f; // 判定用カプセル半径
	int beamSamples_ = 5; // 扇を分割して複数のカプセルで判定
	float beamWaterCost_ = 10.0f;

	// Per-frame delta applied when standing on a moving platform
	Vector3 ridingPlatformDelta_ = {0.0f, 0.0f, 0.0f};

	// Abilities
	Ability currentAbility_ = Ability::Camouflage;
	bool abilityActive_ = false; // for Camouflage: next tongue hit will attempt mimic

	// Sonar parameters
	float sonarRange_ = 10.0f;
	float sonarDuration_ = 3.0f; // seconds (Update uses frames, so will convert)
	float sonarTimer_ = 0.0f;
	float sonarAlpha_ = 0.3f; // alpha applied to revealed objects/enemies

	// Stage reference (for querying objects)
	Stage* stage_ = nullptr;

	// Mimic state
	bool isMimicking_ = false;
	std::string originalModelName_;
	Vector4 originalColor_ = {1.0f,1.0f,1.0f,1.0f};
	Vector3 originalScale_ = {1.0f,1.0f,1.0f};

	std::string mimicModelName_;
	Vector4 mimicColor_ = {1.0f,1.0f,1.0f,1.0f};
	Vector3 mimicScale_ = {1.0f,1.0f,1.0f};

	Vector3 aimTargetPoint_ = { 0.0f, 0.0f, 0.0f };
	bool hasAimTargetPoint_ = false;
};