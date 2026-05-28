#pragma once

#define NOMINMAX
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

#include "3d/Object3d.h"
#include "Tongue.h"
#include "Ability.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include "utility/CollisionUtility.h"
#include "2d/Sprite.h"
#include "math/Vector2.h"
#include "UI/SpriteNumberText.h"

class Camera;
class Object3dCommon;
class CameraController;
class EnemyManager;
class BaseEnemy; // 【追加】前方宣言

class Player {

	enum class MovementState { Root, Jumping, WallClinging, TonguePulling, CeilingCrawling, Warping };

public:
	Player() = default;
	~Player();

	void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& modelName, const Vector3& startPosition = {0.0f, 0.0f, 0.0f});

	void InitializeUI(SpriteCommon* spriteCommon, const std::string& gaugeTextureFilePath = "white.png");

	void InitializeAbilityLevelUI(SpriteCommon* spriteCommon);
	void UpdateAbilityLevelUI();
	void DrawAbilityLevelUI();
	const char* GetAbilityLevelIconTexturePath(AbilityId ability) const;

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

	Vector3 GetHeadbornPosition() const;
	float GetColliderHalfHeight() const { return colliderHalfSize_.y; }

	void SetGroundHeight(float groundHeight) { groundHeight_ = groundHeight; }
	bool IsOnGround() const { return isOnGround_; }

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* blockColliders) { blockColliders_ = blockColliders; }
	void SetBreakableBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) {
		breakableBlockColliders_ = colliders;
	}

	void SetMovementLimitCylinder(const CollisionUtility::Cylinder* cylinder) { movementLimitCylinder_ = cylinder; }

	CollisionUtility::OBB GetPlayerOBB(const Vector3& position) const;

	bool IsWallClinging() const { return moveState_ == MovementState::WallClinging; }
	bool IsCeilingCrawling() const { return moveState_ == MovementState::CeilingCrawling; }
	bool IsClinging() const { return IsWallClinging() || IsCeilingCrawling(); }

	float GetWallClingGauge() const { return wallClingGauge_; }
	float GetMaxWallClingGauge() const { return maxWallClingGauge_; }

	float GetWallClingGaugeRate() const
	{
		if (maxWallClingGauge_ <= 0.0001f) {
			return 0.0f;
		}

		return wallClingGauge_ / maxWallClingGauge_;
	}

	bool IsSonarActive() const { return sonarTimer_ > 0.0f; }
	bool IsTonguePulling() const { return moveState_ == MovementState::TonguePulling; }

	// 水分ゲージ
	float GetWaterGauge() const { return waterGauge_; }
	float GetMaxWaterGauge() const { return maxWaterGauge_; }
	void AddWater(float amount);
	bool ConsumeWater(float amount);

	int GetHP() const { return hp_; }
	int GetMaxHP() const { return maxHp_; }
	bool IsDead() const { return isDead_; }

	void ApplyDamage(int damage);
	void CheckEnemyContactDamage();

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
			tonguePullingEnemy_ = false;
			velocity_ = { 0.0f, 0.0f, 0.0f };

			if (tongue_) {
				tongue_->StartReturn();
			}

			if (moveState_ == MovementState::TonguePulling) {
				TransitionTo(MovementState::Jumping);
			}
		}
	}

	// 現在保持している敵のポインタを返す（マネージャーでの照合用）
	BaseEnemy* GetLastHitEnemy() const { return lastHitEnemy_; }

private:
	bool StartBeamActive(const Vector3& beamDir);
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

	Vector3 GetMoveInputDirection(float cameraYaw) const;
	void UpdateAirborneHorizontalMove(float cameraYaw);

	int GetCurrentChargeLevel() const;
	int GetAllowedChargeLevel() const;
	void ExecuteChargedJump(int chargeLevel);
	void CancelJumpCharge();

	void UpdateWallClinging(float cameraYaw);
	void UpdateCeilingCrawling();
	void UpdateTonguePulling();
	void UpdateWarping();
	void CheckTongueBlockHook();
	bool CheckTongueBlockDamage();

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

	void ResolveHookSurfaceFromPlayerCapsule(
		const CollisionUtility::OBB& block,
		const Vector3& playerPos,
		const Vector3& tongueHitPoint,
		const Vector3& tongueDelta,
		Vector3& outNormal,
		Vector3& outHitPoint
	) const;

	const char* DebugFaceNameFromNormal(const CollisionUtility::OBB& block, const Vector3& normal) const;

	void ResetTongueHitDebug();
	void RecordTongueHitDebug(int step, const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& rawNormal, const Vector3& usedNormal);

	void RefreshClingAnchorFromCurrentSurface();
	void RefreshMovingClingSurfaceFromStage();
	void UpdateClingStageObjectFromHitPoint(const Vector3& hitPoint);
	void ClearClingStageObjectTracking();

	bool IsPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin = 0.0f) const;
	Vector3 ClampPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin = 0.0f) const;

	void UpdateJumpGaugeSprite();
	void UpdateHPGaugeSprite();
	void UpdateWallClingGaugeUISprite();
	void DrawUI();

	void ApplyClingSurfaceRotation();
	void ApplyClingSurfaceRotationFacing(const Vector3& desiredForward);

	void StartJumpPoseAnimation();
	void UpdateJumpPoseAnimation();
	void EndJumpPoseAnimation();

	void SetWallDetachJumpBoost(float boostY);

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

	// XP tuning loaded from resources/ability_config.json
  enum class XPMode {
		Power,
		Odd,
	};
	XPMode xpMode_ = XPMode::Power;
	float xpOddScale_ = 1.0f; // Odd mode: need = oddScale * (2*level - 1)
	float xpBase_ = 50.0f;
	float xpGrowth_ = 2.0f;
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

	int jumpChargeAirCancelGraceFrames_ = 5;
	int jumpChargeAirCancelGraceTimer_ = 0;

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
    float baseJumpPowers_[4] = {0.33f, 0.42f, 0.48f, 0.66f};

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
	const std::vector<CollisionUtility::OBB>* breakableBlockColliders_ = nullptr;
	const CollisionUtility::Cylinder* movementLimitCylinder_ = nullptr;

	Vector3 colliderHalfSize_ = {1.0f, 1.5f, 1.7f};

	// Pending teleport requested by external systems (applied inside Update)
	bool pendingTeleport_ = false;
	Vector3 pendingTeleportPosition_ = {0.0f, 0.0f, 0.0f};

	// ワープ移動用
	Vector3 warpStartPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 warpTargetPosition_ = { 0.0f, 0.0f, 0.0f };
	float warpTimer_ = 0.0f;
	float warpDuration_ = 45.0f; // 45フレーム（0.75秒）かけて移動

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

	EnemyManager* enemyManager_ = nullptr;
	float beamCooldown_ = 60.0f; // frames
	float beamTimer_ = 0.0f;
	float beamRange_ = 8.0f;
	float beamHalfAngleDeg_ = 30.0f; // 扇の半角度
	float beamCapsuleRadius_ = 0.8f; // 判定用カプセル半径
	int beamSamples_ = 5;            // 扇を分割して複数のカプセルで判定
	float beamWaterCost_ = 0.0f;
    // スイープ中の状態
    float beamActiveTimer_ = 0.0f;               
	float beamActiveDuration_ = 45.0f;           
	float beamBaseActiveDuration_ = 45.0f;       
	Vector3 beamActiveOrigin_ = {0.0f,0.0f,0.0f};
	Vector3 beamActiveDir_ = {0.0f,0.0f,1.0f};
	float beamActiveHalfAngleDeg_ = 0.0f;
	float beamActiveMaxRadius_ = 0.0f;
	std::vector<std::pair<BaseEnemy*, int>> beamActiveHitList_;
    // Beam attack の際に使うカプセルの半径は、プレイヤーの当たり判定より少し大きめにして、ヒットさせやすくする
	float beamOriginalCapsuleRadius_ = 0.8f;

	// Per-frame delta applied when standing on a moving platform
	Vector3 ridingPlatformDelta_ = {0.0f, 0.0f, 0.0f};

	// Abilities
	Ability currentAbility_ = Ability::Camouflage;
	bool abilityActive_ = false; // for Camouflage: next tongue hit will attempt mimic

	// Sonar parameters
	float sonarRange_ = 60.0f;
	float sonarDuration_ = 3.0f; // seconds (Update uses frames, so will convert)
	float sonarTimer_ = 0.0f;
	float sonarAlpha_ = 0.3f; // alpha applied to revealed objects/enemies

	// Stage reference (for querying objects)
	Stage* stage_ = nullptr;

	// Mimic state
	bool isMimicking_ = false;
	std::string originalModelName_;
	Vector4 originalColor_ = {0.2f, 0.8f, 0.5f, 1.0f};
	Vector3 originalScale_ = {1.0f, 1.0f, 1.0f};

	std::string mimicModelName_;
	Vector4 mimicColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 mimicScale_ = {1.0f, 1.0f, 1.0f};

	Vector3 aimTargetPoint_ = {0.0f, 0.0f, 0.0f};
	bool hasAimTargetPoint_ = false;

	bool suppressTongueShotThisFrame_ = false;

	bool debugShowRawTongueHit_ = true;
	bool debugIgnoreHookSurfaceCorrection_ = false;
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

	// 地上移動の慣性
	Vector3 groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };

	// ジャンプ開始時に固定する水平移動ベクトル
	Vector3 lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };

	float airControlAccelerationMin_ = 0.004f;
	float airControlAccelerationMax_ = 0.010f;
	float airControlMaxSpeed_ = moveSpeed_;
	float airControlDrag_ = 0.045f;

	float maxFallSpeed_ = 0.60f;

	// 徐々に向きを変えるための設定
	float turnSpeedRad_ = 0.48f;                  // 1フレームで回る最大量
	float moveStartAngleThresholdDeg_ = 12.0f;    // この角度以内になったら前進開始

	// AimMode中、正面と移動方向がズレているときの最低移動速度倍率
	// 1.0なら減速なし、0.55なら横移動・後退が約55%になる
	float aimMoveMinSpeedRate_ = 0.55f;

	// 薙ぎ払い攻撃中はプレイヤー本体の向きを固定する
	bool freezeFacingWhileTongueSweeping_ = true;

	// 今後の加速度寄り設計へ向けた設定
	float groundAcceleration_ = 0.035f;
	float groundDeceleration_ = 0.045f;

	// 最後に舌が当たった敵とその部位ID
	BaseEnemy* lastHitEnemy_ = nullptr;
	int lastHitEnemyPartId_ = 0;

	// ジャンプ関連中は歩きアニメの途中フレームをポーズとして使う
	bool useWalkFrameAsJumpPose_ = true;
	bool jumpPoseAnimationActive_ = false;
	std::string jumpPoseAnimationName_ = "walk";
	int jumpPoseHoldFrame_ = 18;

	std::unique_ptr<Sprite> jumpGaugeBackSprite_ = nullptr;
	std::unique_ptr<Sprite> jumpGaugeFillSprite_ = nullptr;

	Vector2 jumpGaugeOffset_ = { 70.0f, -20.0f };
	Vector2 jumpGaugeSize_ = { 150.0f, 18.0f };

	bool showJumpGauge_ = false;

	std::unique_ptr<Sprite> hpGaugeBackSprite_ = nullptr;
	std::unique_ptr<Sprite> hpGaugeFillSprite_ = nullptr;
	std::unique_ptr<Sprite> hpGaugeFrameSprite_ = nullptr;

	Vector2 hpGaugePos_ = { 40.0f, 32.0f };
	Vector2 hpGaugeSize_ = { 320.0f, 20.0f };
	float hpGaugeFrameThickness_ = 3.0f;

	int maxHp_ = 30;
	int hp_ = 30;

	bool isDead_ = false;

	std::unique_ptr<Sprite> wallClingGaugeBackSprite_ = nullptr;
	std::unique_ptr<Sprite> wallClingGaugeFillSprite_ = nullptr;

	Vector2 wallClingGaugePos_ = { 40.0f, 60.0f };   // HPバーの下に置く
	Vector2 wallClingGaugeSize_ = { 320.0f, 14.0f };
	bool showWallClingGaugeUI_ = true;

	struct AbilityLevelUIEntry {
		AbilityId ability = AbilityId::Unknown;
		std::unique_ptr<Sprite> iconSprite = nullptr;
		std::unique_ptr<Sprite> xpFillSprite = nullptr;
		std::unique_ptr<Sprite> lvPrefixSprite = nullptr;
		std::unique_ptr<Sprite> stateOverlaySprite = nullptr;
		SpriteNumberText levelNumberText;
	};

	std::vector<AbilityLevelUIEntry> abilityLevelUIEntries_;

	std::array<AbilityId, 5> abilityLevelUIOrder_ = {
		AbilityId::JumpPower,
		AbilityId::SonarDuration,
		AbilityId::WallClingDuration,
		AbilityId::TongueRange,
		AbilityId::CamouflageDuration
	};

	bool showAbilityLevelUI_ = true;

	// 右下寄せ用
	float abilityLevelUIRightMargin_ = 24.0f;
	float abilityLevelUIBottomMargin_ = -10.0f;

	// 1項目あたり
	Vector2 abilityLevelUIIconSize_ = { 56.0f, 56.0f };
	float abilityLevelUIVerticalGap_ = -36.0f;

	// アイコンの下の "lv." 表示
	Vector2 abilityLevelUILevelTextOffset_ = { 0.0f, 60.0f };
	Vector2 abilityLevelUILvPrefixSize_ = { 26.0f, 14.0f };
	Vector2 abilityLevelUINumberSize_ = { 16.0f, 16.0f };

	// XP進捗の色
	Vector4 abilityLevelUIXPFillColor_ = { 0.2f, 0.8f, 1.0f, 0.30f };

	// "lv." の生成先
	std::string abilityLevelUILvPrefixTexturePath_ = "resources/UI/generated/ability_lv_prefix.png";

	float abilityLevelUIBlinkTimer_ = 0.0f;
	Vector4 mimicAbilityOnColor_ = { 1.0f, 1.0f, 0.0f, 0.78f };
	float mimicAbilityOnOverlayExpand_ = 8.0f;

	// 接触ダメージ設定
	int enemyContactDamage_ = 1;
	float enemyContactInvincibilityFrames_ = 30.0f;
	float enemyContactInvincibilityTimer_ = 0.0f;

	bool tonguePullingEnemy_ = false;

	// 移動床が上昇している時、壁張り付き離脱ジャンプだけに乗せる上昇分
	float wallDetachJumpBoostY_ = 0.0f;

	// delta.y は1フレームあたりの移動量なので、そのまま足す
	float wallDetachJumpBoostScale_ = 1.0f;

	// 強くなりすぎ防止
	float wallDetachJumpBoostMax_ = 20.0f;
};