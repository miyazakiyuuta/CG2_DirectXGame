#pragma once

#define NOMINMAX
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "3d/Object3d.h"
#include "Tongue.h"
#include "Ability.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"

class Camera;
class Object3dCommon;
class CameraController;
class EnemyManager;
class BaseEnemy; // 【追加】前方宣言

class Player {

	enum class MovementState { Root, Jumping, WallClinging, TonguePulling, CeilingCrawling };

public:
	Player() = default;
	~Player();

	void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& modelName, const Vector3& startPosition = {0.0f, 0.0f, 0.0f});

	void Update();
	void Draw();
	void DrawImGui();

	void SetCamera(Camera* camera);
	void SetCameraController(CameraController* cameraController) { cameraController_ = cameraController; }

	void SetPosition(const Vector3& position);

	// 速度低下デバフを外部からセットする
	void SetSpeedMultiplier(float mul) { speedMultiplier_ = mul; }

	// Request a teleport to be applied on the next Player::Update call.
	// Using this avoids other systems overwriting the teleport by
	// ensuring the player applies it inside its own update logic.
	void SetPendingTeleport(const Vector3& position);

	Vector3 GetPosition() const;
	Vector3 GetRotate() const;
	Vector3 GetVelocity() const { return velocity_; }
	void SetVelocity(const Vector3& v) { velocity_ = v; }
	// Moving platform support: set per-frame platform delta applied to player
	void SetRidingPlatformDelta(const Vector3& delta);
	void ClearRidingPlatformDelta();

	void SetGroundHeight(float groundHeight) { groundHeight_ = groundHeight; }
	bool IsOnGround() const { return isOnGround_; }

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* blockColliders) { blockColliders_ = blockColliders; }

	void SetMovementLimitCylinder(const CollisionUtility::Cylinder* cylinder) { movementLimitCylinder_ = cylinder; }

	CollisionUtility::OBB GetPlayerOBB(const Vector3& position) const;

	// 水分ゲージ
	float GetWaterGauge() const { return waterGauge_; }
	float GetMaxWaterGauge() const { return maxWaterGauge_; }
	void AddWater(float amount);
	bool ConsumeWater(float amount);

	// チャージジャンプ
	float GetJumpChargeRate() const;
	int GetCurrentVisibleChargeLevel() const;
	int GetChargeStock() const { return chargeStock_; }
	void AddChargeStock(int amount);
	void SetChargeStock(int stock);
	int GetCurrentChargePhase() const;
	float GetCurrentChargePhaseRate() const;
	bool IsChargeAtMaxPhase() const;

	float GetYaw() const;
	Tongue* GetTongue() const { return tongue_.get(); }

	// Abilities
	enum class Ability {
		Camouflage,
		Sonar,
	};

	Ability GetCurrentAbility() const { return currentAbility_; }
	void SetStage(class Stage* stage) { stage_ = stage; }

	// Mimic control
	void EndMimic() {
		if (!object_)
			return;
		// restore original visual
		object_->SetModel(originalModelName_);
		object_->SetScale(originalScale_);
		Vector4 c = originalColor_;
		c.w = currentAlpha_;
		object_->SetColor(c);
		isMimicking_ = false;
	}

	void UpdateTransparencyByCamera(const Vector3& cameraPosition);
	float GetCurrentAlpha() const { return currentAlpha_; }

	void SetAlpha(float alpha) { currentAlpha_ = alpha; }

	bool TryShotTongue(const Vector3& direction);
	bool TryUseBeam(const Vector3& direction);
	void SetYawFromCamera(float cameraYaw);
	void SetEnemyManager(EnemyManager* mgr) { enemyManager_ = mgr; }

	void SetAimTargetPoint(const Vector3& point) {
		aimTargetPoint_ = point;
		hasAimTargetPoint_ = true;
	}

	void ClearAimTargetPoint() { hasAimTargetPoint_ = false; }

	bool IsMimicking() const { return isMimicking_; }

	// 敵が死んだことを外部（EnemyManager）から通知して、ポインタを安全にクリアする
	void NotifyEnemyDead(BaseEnemy* deadEnemy) {
		if (lastHitEnemy_ == deadEnemy) {
			lastHitEnemy_ = nullptr;
		}
	}

	// 現在保持している敵のポインタを返す（マネージャーでの照合用）
	BaseEnemy* GetLastHitEnemy() const { return lastHitEnemy_; }

private:
    // 能力の状態を管理する構造体
	struct AbilityState {
		int level = 1;
		float xp = 0.0f;
		int maxLevel = 10;
	};

    // 現在の能力レベルとXPを管理するマップ
	std::unordered_map<AbilityId, AbilityState> abilityStates_;

	// 現在の能力選択
	float XPToNextLevel(int level) const;

public:
    // 外部から能力XPをキューイングするための関数（敵のドロップなどから呼ばれる）
	void EnqueueAbilityXP(AbilityId ability, float amount);
	void MoveHorizontal(float cameraYaw);
	void UpdateJumpCharge();
	void ApplyGravity();

	int GetCurrentChargeLevel() const;
	int GetAllowedChargeLevel() const;
	void ExecuteChargedJump(int chargeLevel);
	void CancelJumpCharge();

	void UpdateWallClinging(float cameraYaw);
	void UpdateCeilingCrawling();
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

	bool TryReattachToAdjacentSurface(const Vector3& fromPosition, const Vector3& moveDir, Vector3& outPosition);

	void ResolveMovementLimitCylinder();

	bool CanStartTongueShot() const;

	bool IsGroundSurface(const Vector3& normal) const;
	bool IsCeilingSurface(const Vector3& normal) const;
	bool IsWallSurface(const Vector3& normal) const;

    Vector3 ResolveHookSurfaceNormal(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& tongueDelta, const Vector3& playerPos) const;

	Vector3 ResolveHookSurfaceNormalFromPlayerCapsule(const CollisionUtility::OBB& block, const Vector3& playerPos, const Vector3& hitPoint, const Vector3& tongueDelta) const;

	const char* DebugFaceNameFromNormal(const CollisionUtility::OBB& block, const Vector3& normal) const;

	void ResetTongueHitDebug();
	void RecordTongueHitDebug(int step, const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& rawNormal, const Vector3& usedNormal);

	void RefreshClingAnchorFromCurrentSurface();
	void RefreshMovingClingSurfaceFromStage();
	void UpdateClingStageObjectFromHitPoint(const Vector3& hitPoint);
	void ClearClingStageObjectTracking();

private:
        // 経験値とレベルアップの保留キュー
	std::vector<std::pair<AbilityId, float>> pendingAbilityXP_;
	std::vector<std::pair<AbilityId, int>> pendingLevelUps_;

	// ApplyPendingAbilityXP を呼ぶ前に、pendingLevelUps_ にレベルアップ分の処理を追加する
	void ApplyPendingAbilityXP();
    // ApplyPendingAbilityXP 内でレベルアップが発生した場合の処理（レベルアップの反映と、レベルアップに伴うXPの減算）
	void ApplyPendingLevelUps();

public:
    // 外部から直接能力XPを加算するための関数（テスト用など。通常は EnqueueAbilityXP を使うべき）
	void AddAbilityXP(AbilityId ability, float amount);
	std::unique_ptr<Object3d> object_ = nullptr;
	Camera* camera_ = nullptr;
	CameraController* cameraController_ = nullptr;
	Input* input_ = nullptr;

	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	Vector3 lastMove_ = {0.0f, 0.0f, 0.0f};

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

    // チャージジャンプ (old jumpPowers_ removed; use baseJumpPowers_)
	float chargeThresholds_[3] = {40.0f, 80.0f, 120.0f};
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
	float ceilingCrawlSpeed_ = 0.35f;

	// 能力強化の倍率（1.0が通常）
	float jumpPowerMultiplier_ = 1.0f;
	float baseJumpPowers_[4] = {0.55f, 0.70f, 0.80f, 1.10f};

	float tongueRangeMultiplier_ = 1.0f;
	float baseTongueMaxDistance_ = 30.0f; // will be synced to Tongue

	float sonarDurationMultiplier_ = 1.0f;
	float baseSonarDuration_ = 3.0f;

	float wallClingDurationMultiplier_ = 1.0f;
	float baseWallClingGauge_ = 100.0f;

	float camouflageDurationMultiplier_ = 1.0f;
	float baseCamouflageDuration_ = 180.0f; // frames
	float mimicTimer_ = 0.0f; // frames remaining for mimic

	// 能力強化の設定がロードされたか
	bool abilityConfigLoaded_ = false;

	// レベルアップごとの倍率増加量
	float jumpPowerPerLevel_ = 0.10f; // default +10% per level
	float tongueRangePerLevel_ = 0.08f; // default +8% per level
	float sonarDurationPerLevel_ = 0.10f;
	float wallClingPerLevel_ = 0.10f;
	float camouflagePerLevel_ = 0.10f;

	void LoadAbilityConfig();

	Vector3 wallRightVec_ = {1.0f, 0.0f, 0.0f};

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;
	const CollisionUtility::Cylinder* movementLimitCylinder_ = nullptr;

	Vector3 colliderHalfSize_ = {1.0f, 1.0f, 1.0f};

	// Pending teleport requested by external systems (applied inside Update)
	bool pendingTeleport_ = false;
	Vector3 pendingTeleportPosition_ = {0.0f, 0.0f, 0.0f};

	// 舌で引っ張られる処理
	Vector3 tonguePullTarget_ = {0.0f, 0.0f, 0.0f};
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

	Vector3 tongueHookNormal_ = {0.0f, 0.0f, -1.0f};

	bool hasClingSurface_ = false;
	CollisionUtility::OBB clingBlockObb_ = {};
	Vector3 clingSurfaceNormal_ = {0.0f, 0.0f, -1.0f};
	Vector3 clingSurfaceRight_ = {1.0f, 0.0f, 0.0f};
	Vector3 clingSurfaceUp_ = {0.0f, 1.0f, 0.0f};
	Vector3 clingSurfaceCenter_ = {0.0f, 0.0f, 0.0f};

	float clingSurfaceHalfWidth_ = 0.0f;
	float clingSurfaceHalfHeight_ = 0.0f;

	float clingHitRightOffset_ = 0.0f;
	float clingHitUpOffset_ = 0.0f;

	float wallDetachMargin_ = 0.05f;
	float wallKeepDistance_ = 0.03f;
	float clingReattachSearchDistance_ = 1.2f;

	bool useTonguePull_ = true;

	float clingGroundNormalThreshold_ = 0.6f;
	float clingCeilingNormalThreshold_ = 0.6f;

	// Beam attack (扇状薙ぎ払い) parameters
	EnemyManager* enemyManager_ = nullptr;
	float beamCooldown_ = 60.0f; // frames
	float beamTimer_ = 0.0f;
	float beamRange_ = 8.0f;
	float beamHalfAngleDeg_ = 30.0f; // 扇の半角度
	float beamCapsuleRadius_ = 0.8f; // 判定用カプセル半径
	int beamSamples_ = 5;            // 扇を分割して複数のカプセルで判定
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
	Vector4 originalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 originalScale_ = {1.0f, 1.0f, 1.0f};

	std::string mimicModelName_;
	Vector4 mimicColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 mimicScale_ = {1.0f, 1.0f, 1.0f};

	Vector3 aimTargetPoint_ = {0.0f, 0.0f, 0.0f};
	bool hasAimTargetPoint_ = false;

	bool suppressTongueShotThisFrame_ = false;

	bool debugShowRawTongueHit_ = true;
	bool debugIgnoreHookSurfaceCorrection_ = true;
	bool debugIgnoreGroundRejectOnRawHit_ = true;

	bool hasTongueHitDebug_ = false;
	int debugTongueHitStep_ = -1;

	Vector3 debugTongueHitPoint_ = {0.0f, 0.0f, 0.0f};
	Vector3 debugTongueRawNormal_ = {0.0f, 0.0f, 0.0f};
	Vector3 debugTongueUsedNormal_ = {0.0f, 0.0f, 0.0f};

	const char* debugTongueRawFaceName_ = "None";
	const char* debugTongueUsedFaceName_ = "None";

	// 移動速度倍率（1.0が通常）
	float speedMultiplier_ = 1.0f;

	int clingStageObjectId_ = -1;
	bool clingStageObjectIsMovingPlatform_ = false;

	// 最後に舌がヒットしたエネミーを保持
	BaseEnemy* lastHitEnemy_ = nullptr;
};