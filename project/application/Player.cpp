#include "Player.h"

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include "CameraController.h"
#include "Enemy/Core/BaseEnemy.h"
#include "Enemy/Manager/EnemyManager.h"
#include "Stage.h"
#include "math/Vector4.h"
#include "utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <../externals/nlohmann/json.hpp>
#include "2d/SpriteCommon.h"

Player::~Player() = default;

namespace {
float LengthXZ(const Vector3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }

float Length3(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3 Normalize3(const Vector3& v) {
	float len = Length3(v);
	if (len <= 0.0001f) {
		return {0.0f, 0.0f, 1.0f};
	}
	return {v.x / len, v.y / len, v.z / len};
}

static Vector3 RotateY(const Vector3& v, float angleRad) {
	float s = std::sin(angleRad);
	float c = std::cos(angleRad);
	return {v.x * c - v.z * s, v.y, v.x * s + v.z * c};
}

static float PointToSegmentDistSq(const Vector3& p, const Vector3& a, const Vector3& b) {
	Vector3 ab = b - a;
	Vector3 ap = p - a;
	float abLen2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
	if (abLen2 <= 1e-6f) {
		Vector3 d = p - a;
		return d.x * d.x + d.y * d.y + d.z * d.z;
	}
	float t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / abLen2;
	t = std::max(0.0f, std::min(1.0f, t));
	Vector3 proj = {a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
	Vector3 d = p - proj;
	return d.x * d.x + d.y * d.y + d.z * d.z;
}

bool WorldToScreenSimple(
	const Vector3& worldPos,
	const Matrix4x4& viewProjection,
	float screenW,
	float screenH,
	Vector2& outScreen){
	Vector4 clip{};
	clip.x = worldPos.x * viewProjection.m[0][0] + worldPos.y * viewProjection.m[1][0] + worldPos.z * viewProjection.m[2][0] + 1.0f * viewProjection.m[3][0];
	clip.y = worldPos.x * viewProjection.m[0][1] + worldPos.y * viewProjection.m[1][1] + worldPos.z * viewProjection.m[2][1] + 1.0f * viewProjection.m[3][1];
	clip.z = worldPos.x * viewProjection.m[0][2] + worldPos.y * viewProjection.m[1][2] + worldPos.z * viewProjection.m[2][2] + 1.0f * viewProjection.m[3][2];
	clip.w = worldPos.x * viewProjection.m[0][3] + worldPos.y * viewProjection.m[1][3] + worldPos.z * viewProjection.m[2][3] + 1.0f * viewProjection.m[3][3];

	if(clip.w <= 0.0001f){
		return false;
	}

	float ndcX = clip.x / clip.w;
	float ndcY = clip.y / clip.w;

	outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
	outScreen.y = (-ndcY * 0.5f + 0.5f) * screenH;
	return true;
}

static float WrapAnglePi(float angle) {
	while (angle > 3.14159265f) {
		angle -= 6.28318530f;
	}
	while (angle < -3.14159265f) {
		angle += 6.28318530f;
	}
	return angle;
}

static float ApproachFloat(float current, float target, float delta) {
	if (current < target) {
		return std::min(current + delta, target);
	}
	return std::max(current - delta, target);
}

} // namespace

// 次のレベルまでの必要経験値を計算する関数。レベルが上がるごとに必要経験値が増えるように、二次関数的な成長をさせています。
float Player::XPToNextLevel(int level) const {
	const float base = 50.0f;
	return base * static_cast<float>(level) * static_cast<float>(level);
}

// 指定した能力に経験値を追加し、必要に応じてレベルアップを処理する関数
void Player::AddAbilityXP(AbilityId ability, float amount) {
	if (amount <= 0.0f)
		return;

	auto& s = abilityStates_[ability];
        // maxLevel が 0 以下の場合はデフォルトの最大レベルを設定する（安全策）
	if (s.maxLevel <= 0)
		s.maxLevel = 10;
	if (s.level >= s.maxLevel) {
		s.xp = 0.0f;
		return;
	}

	s.xp += amount;

	int oldLevel = s.level;

	// レベルアップ処理: 現在のXPが次のレベルに必要なXPを超えている限り、レベルを上げてXPを減らす
	while (s.level < s.maxLevel) {
		float need = XPToNextLevel(s.level);
		if (s.xp >= need) {
			s.xp -= need;
			s.level += 1;
		} else {
			break;
		}
	}

	// レベルが上がりすぎないように、maxLevel を超えたらレベルを maxLevel に固定してXPをリセットする
	if (s.level >= s.maxLevel) {
		s.level = s.maxLevel;
		s.xp = 0.0f;
	}

	// 現在のレベルが追加前より上がっていたら、pendingLevelUps_ に追加して、後でまとめて反映する
    if (s.level > oldLevel) {
        // 追加前のレベルと追加後のレベルを両方記録しておく（反映時に両方使う可能性があるため）
		pendingLevelUps_.push_back({ability, s.level});
	}
}

void Player::EnqueueAbilityXP(AbilityId ability, float amount) {
	if (amount <= 0.0f)
		return;
	pendingAbilityXP_.push_back({ability, amount});
}

void Player::ApplyPendingAbilityXP() {
	if (pendingAbilityXP_.empty())
		return;

	for (auto &p : pendingAbilityXP_) {
		AddAbilityXP(p.first, p.second);
	}
	pendingAbilityXP_.clear();
}

void Player::ApplyPendingLevelUps() {
	if (pendingLevelUps_.empty())
		return;

	for (auto &lv : pendingLevelUps_) {
        AbilityId ability = lv.first;
		int newLevel = lv.second;

		// レベルアップに伴う能力の強化を反映する
        if (ability == AbilityId::JumpPower) {
			jumpPowerMultiplier_ = 1.0f + jumpPowerPerLevel_ * static_cast<float>(newLevel - 1);
		} else if (ability == AbilityId::TongueRange) {
			tongueRangeMultiplier_ = 1.0f + tongueRangePerLevel_ * static_cast<float>(newLevel - 1);
			if (tongue_) {
				float newMax = baseTongueMaxDistance_ * tongueRangeMultiplier_;
				tongue_->SetMaxDistance(newMax);
			}
        } else if (ability == AbilityId::SonarDuration) {
			sonarDurationMultiplier_ = 1.0f + sonarDurationPerLevel_ * static_cast<float>(newLevel - 1);
			sonarDuration_ = baseSonarDuration_ * sonarDurationMultiplier_;
		} else if (ability == AbilityId::WallClingDuration) {
			wallClingDurationMultiplier_ = 1.0f + wallClingPerLevel_ * static_cast<float>(newLevel - 1);
			maxWallClingGauge_ = baseWallClingGauge_ * wallClingDurationMultiplier_;
			if (wallClingGauge_ > maxWallClingGauge_)
				wallClingGauge_ = maxWallClingGauge_;
		} else if (ability == AbilityId::CamouflageDuration) {
			camouflageDurationMultiplier_ = 1.0f + camouflagePerLevel_ * static_cast<float>(newLevel - 1);
		} else {
           // 不明な能力のレベルアップは無視する（安全策）
		}
	}

	pendingLevelUps_.clear();
}

void Player::LoadAbilityConfig() {
	if (abilityConfigLoaded_)
		return;

	try {
		const std::string path = "resources/ability_config.json";
		if (std::filesystem::exists(path)) {
			std::ifstream ifs(path);
			if (ifs.is_open()) {
				nlohmann::json j;
				ifs >> j;
				if (j.contains("JumpPower")) {
					auto &o = j["JumpPower"];
					if (o.contains("perLevel")) jumpPowerPerLevel_ = o["perLevel"].get<float>();
				}
				if (j.contains("TongueRange")) {
					auto &o = j["TongueRange"];
					if (o.contains("base")) baseTongueMaxDistance_ = o["base"].get<float>();
					if (o.contains("perLevel")) tongueRangePerLevel_ = o["perLevel"].get<float>();
				}
				if (j.contains("SonarDuration")) {
					auto &o = j["SonarDuration"];
					if (o.contains("base")) baseSonarDuration_ = o["base"].get<float>();
					if (o.contains("perLevel")) sonarDurationPerLevel_ = o["perLevel"].get<float>();
				}
				if (j.contains("WallClingDuration")) {
					auto &o = j["WallClingDuration"];
					if (o.contains("base")) baseWallClingGauge_ = o["base"].get<float>();
					if (o.contains("perLevel")) wallClingPerLevel_ = o["perLevel"].get<float>();
				}
				if (j.contains("CamouflageDuration")) {
					auto &o = j["CamouflageDuration"];
					if (o.contains("base")) baseCamouflageDuration_ = o["base"].get<float>();
					if (o.contains("perLevel")) camouflagePerLevel_ = o["perLevel"].get<float>();
				}
			}
		}
	} catch (...) {
            // 例外が発生した場合は、デフォルト値のままにしておく
	}

	// 範囲外のレベルに対しても正しく動作するように、初期化時点で倍率を計算しておく
	sonarDuration_ = baseSonarDuration_ * sonarDurationMultiplier_;
	maxWallClingGauge_ = baseWallClingGauge_ * wallClingDurationMultiplier_;
	abilityConfigLoaded_ = true;
}

bool Player::TryUseBeam(const Vector3& direction) {
	if (beamTimer_ > 0.0f)
		return false;
	if (!enemyManager_)
		return false;
	if (!ConsumeWater(beamWaterCost_))
		return false;

	// Trigger cooldown
	beamTimer_ = beamCooldown_;

	// Prepare sweep
	Vector3 origin = GetPosition();
	// Slight upward offset so capsule is not ground clipping
	origin.y += 0.5f;

	float halfAngleRad = beamHalfAngleDeg_ * (3.14159265f / 180.0f);
	int samples = std::max(1, beamSamples_);

	std::vector<BaseEnemy*> hitList;

	for (int i = 0; i < samples; ++i) {
		float t = (samples == 1) ? 0.0f : (static_cast<float>(i) / static_cast<float>(samples - 1));
		float angle = -halfAngleRad + t * (2.0f * halfAngleRad);
		Vector3 dir = Normalize3(direction);
		Vector3 sweepDir = RotateY(dir, angle);
		Vector3 segEnd = origin + sweepDir * beamRange_;

		// Check against enemies
		enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
			if (!e)
				return;
			// avoid double-hitting
			for (auto h : hitList)
				if (h == e)
					return;

			Vector3 ep = e->GetPosition();
			float distSq = PointToSegmentDistSq(ep, origin, segEnd);
			const float enemyRadius = 0.7f; // approximate
			float hitRadius = beamCapsuleRadius_ + enemyRadius;
			if (distSq <= hitRadius * hitRadius) {
				hitList.push_back(e);
			}
		});
	}

	// Apply effect: kill hit enemies (minimal implementation)
	for (auto e : hitList) {
		if (e)
			e->Kill();
	}

	return true;
}

float Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float ComputeOBBSupportRadiusAlongNormal(const CollisionUtility::OBB& obb, const Vector3& normal) {
	return std::abs(Dot3(obb.axis[0], normal)) * obb.halfLength[0] + std::abs(Dot3(obb.axis[1], normal)) * obb.halfLength[1] + std::abs(Dot3(obb.axis[2], normal)) * obb.halfLength[2];
}

float AbsDot3(const Vector3& a, const Vector3& b) { return std::abs(Dot3(a, b)); }

Vector3 Cross3(const Vector3& a, const Vector3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

float ClampFloat(float v, float minV, float maxV) { return std::max(minV, std::min(v, maxV)); }

static Vector3 ExtractEulerXYZFromBasis(
	const Vector3& right,
	const Vector3& up,
	const Vector3& forward
) {
	// Matrix4x4::Rotate() は RotateX * RotateY * RotateZ 規約
	// row0 = right, row1 = up, row2 = forward として Euler を復元する
	const float m00 = right.x;
	const float m01 = right.y;
	const float m02 = right.z;

	const float m10 = up.x;
	const float m11 = up.y;
	const float m12 = up.z;

	const float m20 = forward.x;
	const float m21 = forward.y;
	const float m22 = forward.z;

	float rotY = std::asin(std::clamp(-m02, -1.0f, 1.0f));
	float cosY = std::cos(rotY);

	float rotX = 0.0f;
	float rotZ = 0.0f;

	if (std::abs(cosY) > 0.0001f) {
		rotX = std::atan2(m12, m22);
		rotZ = std::atan2(m01, m00);
	}
	else {
		// gimbal lock 付近の簡易フォールバック
		rotX = std::atan2(-m21, m11);
		rotZ = 0.0f;
	}

	return { rotX, rotY, rotZ };
}

void Player::Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& modelName, const Vector3& startPosition) {
	camera_ = camera;
	input_ = Input::GetInstance();

	object_ = std::make_unique<Object3d>();
	object_->Initialize(object3dCommon);
	object_->SetModel(modelName);
	object_->SetCamera(camera_);
	object_->SetTranslate(startPosition);
	object_->SetRotate({0.0f, 0.0f, 0.0f});
	object_->SetScale({1.5f, 1.5f, 1.5f});

	// store original visual info for mimic restore
	originalModelName_ = modelName;
	originalScale_ = object_->GetScale();
	originalColor_ = {0.2f, 0.8f, 0.5f, 1.0f};

	tongue_ = std::make_unique<Tongue>();
	tongue_->Initialize(object3dCommon, camera_, this, "tongue/tongue.obj");

	velocity_ = {0.0f, 0.0f, 0.0f};
	lastMove_ = {0.0f, 0.0f, 0.0f};
	isOnGround_ = false;

	waterGauge_ = maxWaterGauge_;

	isChargingJump_ = false;
	isJumpChargeCanceled_ = false;
	chargeTimer_ = 0.0f;
	chargeMaxHoldTimer_ = 0.0f;
	moveState_ = MovementState::Root;
	wallClingGauge_ = maxWallClingGauge_;
	prevAimMode_ = false;
	InitializeUI(SpriteCommon::GetInstance(), "white");

}

void Player::InitializeUI(SpriteCommon* spriteCommon, const std::string& gaugeTextureFilePath){
	if(!spriteCommon){
		return;
	}

	jumpGaugeBackSprite_ = std::make_unique<Sprite>();
	jumpGaugeBackSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
	jumpGaugeBackSprite_->SetSize(jumpGaugeSize_);
	jumpGaugeBackSprite_->SetColor({ 0.08f, 0.08f, 0.08f, 0.65f });

	jumpGaugeFillSprite_ = std::make_unique<Sprite>();
	jumpGaugeFillSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
	jumpGaugeFillSprite_->SetSize({ 0.0f, jumpGaugeSize_.y });
	jumpGaugeFillSprite_->SetColor({ 0.3f, 1.0f, 0.2f, 0.9f });

	showJumpGauge_ = false;
}

void Player::ResolveMovementLimitCylinder() {
	if (!object_ || !movementLimitCylinder_) {
		return;
	}

	Vector3 position = object_->GetTranslate();

	if (CollisionUtility::IsPointInsideCylinder(position, *movementLimitCylinder_)) {
		return;
	}

	position = CollisionUtility::ClosestPointInsideCylinder(position, *movementLimitCylinder_);

	// 念のため少しだけ内側へ寄せる
	Vector3 toCenter = {movementLimitCylinder_->center.x - position.x, 0.0f, movementLimitCylinder_->center.z - position.z};

	float lenSq = toCenter.x * toCenter.x + toCenter.z * toCenter.z;
	if (lenSq > 1e-8f) {
		float invLen = 1.0f / std::sqrt(lenSq);
		const float kInsideBias = 0.01f;
		position.x += toCenter.x * invLen * kInsideBias;
		position.z += toCenter.z * invLen * kInsideBias;
	}

	object_->SetTranslate(position);

	// 外へ押し続ける水平速度があると壁際で暴れやすいので少し落とす
	velocity_.x = 0.0f;
	velocity_.z = 0.0f;
}

bool Player::CanStartTongueShot() const {
	if (!tongue_) {
		return false;
	}

	// すでに舌が発射状態なら不可
	// Idle 以外は Extending / Hooked / Returning を含めて全部禁止
	if (tongue_->GetState() != Tongue::State::Idle) {
		return false;
	}

	// 舌で引っ張られている間や壁張り付き中も止めたいならここで追加できる
	if (moveState_ == MovementState::TonguePulling || moveState_ == MovementState::WallClinging || moveState_ == MovementState::CeilingCrawling) {
		return false;
	}

	return true;
}

bool Player::IsGroundSurface(const Vector3& normal) const { return normal.y >= clingGroundNormalThreshold_; }

bool Player::IsCeilingSurface(const Vector3& normal) const { return normal.y <= -clingCeilingNormalThreshold_; }

bool Player::IsWallSurface(const Vector3& normal) const { return !IsGroundSurface(normal) && !IsCeilingSurface(normal); }

Vector3 Player::ResolveHookSurfaceNormal(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& tongueDelta, const Vector3& playerPos) const {
	Vector3 axis[3] = {
	    Normalize3(block.axis[0]),
	    Normalize3(block.axis[1]),
	    Normalize3(block.axis[2]),
	};

	Vector3 localVec = hitPoint - block.center;
	float local[3] = {
	    Dot3(localVec, axis[0]),
	    Dot3(localVec, axis[1]),
	    Dot3(localVec, axis[2]),
	};

	struct Candidate {
		Vector3 normal;
		float distance;
	};

	Candidate c[6] = {
	    {axis[0],      std::abs(block.halfLength[0] - local[0]) },
        {axis[0] * -1, std::abs(-block.halfLength[0] - local[0])},
        {axis[1],      std::abs(block.halfLength[1] - local[1]) },
	    {axis[1] * -1, std::abs(-block.halfLength[1] - local[1])},
        {axis[2],      std::abs(block.halfLength[2] - local[2]) },
        {axis[2] * -1, std::abs(-block.halfLength[2] - local[2])},
	};

	int nearest = 0;
	int second = 1;
	if (c[second].distance < c[nearest].distance) {
		std::swap(nearest, second);
	}

	for (int i = 2; i < 6; ++i) {
		if (c[i].distance < c[nearest].distance) {
			second = nearest;
			nearest = i;
		} else if (c[i].distance < c[second].distance) {
			second = i;
		}
	}

	Vector3 positionBasedNormal = c[nearest].normal;
	float ambiguousMargin = c[second].distance - c[nearest].distance;

	Vector3 travelDir = Normalize3(tongueDelta);
	Vector3 directionBasedNormal = positionBasedNormal;
	{
		float bestScore = -1.0e9f;
		for (int i = 0; i < 3; ++i) {
			Vector3 a = axis[i];
			Vector3 b = axis[i] * -1.0f;

			float scoreA = Dot3(a, travelDir * -1.0f);
			if (scoreA > bestScore) {
				bestScore = scoreA;
				directionBasedNormal = a;
			}

			float scoreB = Dot3(b, travelDir * -1.0f);
			if (scoreB > bestScore) {
				bestScore = scoreB;
				directionBasedNormal = b;
			}
		}
	}

	Vector3 toPlayer = Normalize3(playerPos - hitPoint);
	Vector3 playerBasedNormal = positionBasedNormal;
	{
		float bestScore = -1.0e9f;
		for (int i = 0; i < 3; ++i) {
			Vector3 a = axis[i];
			Vector3 b = axis[i] * -1.0f;

			float scoreA = Dot3(a, toPlayer);
			if (scoreA > bestScore) {
				bestScore = scoreA;
				playerBasedNormal = a;
			}

			float scoreB = Dot3(b, toPlayer);
			if (scoreB > bestScore) {
				bestScore = scoreB;
				playerBasedNormal = b;
			}
		}
	}

	Vector3 resolved = positionBasedNormal;

	const float kAmbiguousThreshold = 0.05f;
	if (ambiguousMargin < kAmbiguousThreshold) {
		resolved = directionBasedNormal;
	}

	if (IsWallSurface(positionBasedNormal) && IsGroundSurface(directionBasedNormal)) {
		resolved = directionBasedNormal;
	}

	if (Dot3(resolved, directionBasedNormal) < 0.5f && Dot3(playerBasedNormal, directionBasedNormal) > 0.5f) {
		resolved = directionBasedNormal;
	}

	return Normalize3(resolved);
}

Vector3 Player::ResolveHookSurfaceNormalFromPlayerCapsule(const CollisionUtility::OBB& block, const Vector3& playerPos, const Vector3& hitPoint, const Vector3& tongueDelta) const {
	Vector3 axis[3] = {
	    Normalize3(block.axis[0]),
	    Normalize3(block.axis[1]),
	    Normalize3(block.axis[2]),
	};

	Vector3 segmentDir = Normalize3(hitPoint - playerPos);
	Vector3 travelDir = Normalize3(tongueDelta);

	Vector3 bestNormal = {0.0f, 1.0f, 0.0f};
	float bestScore = -1.0e9f;

	// 線分の中点も使って、どの面側に近いかを見る
	Vector3 midPoint = {(playerPos.x + hitPoint.x) * 0.5f, (playerPos.y + hitPoint.y) * 0.5f, (playerPos.z + hitPoint.z) * 0.5f};

	Vector3 localMidVec = midPoint - block.center;
	float localMid[3] = {
	    Dot3(localMidVec, axis[0]),
	    Dot3(localMidVec, axis[1]),
	    Dot3(localMidVec, axis[2]),
	};

	for (int i = 0; i < 3; ++i) {
		Vector3 candidates[2] = {axis[i], axis[i] * -1.0f};

		for (int j = 0; j < 2; ++j) {
			Vector3 n = candidates[j];

			// プレイヤー→hitPoint の進行に対して入口面らしいか
			float frontScore = Dot3(n, segmentDir * -1.0f);

			// 舌の進行方向ともある程度整合しているか
			float tongueScore = Dot3(n, travelDir * -1.0f);

			// 線分中点がその面寄りにあるか
			float sideScore = 0.0f;
			if (i == 0) {
				sideScore = (j == 0) ? localMid[0] : -localMid[0];
			} else if (i == 1) {
				sideScore = (j == 0) ? localMid[1] : -localMid[1];
			} else {
				sideScore = (j == 0) ? localMid[2] : -localMid[2];
			}

			// 合成スコア
			float score = frontScore * 0.60f + tongueScore * 0.25f + sideScore * 0.15f;

			// 下方向ショットで壁面を選びにくくする
			if (segmentDir.y < -0.4f && IsWallSurface(n)) {
				score -= 0.5f;
			}

			if (score > bestScore) {
				bestScore = score;
				bestNormal = n;
			}
		}
	}

	return Normalize3(bestNormal);
}

void Player::SetCamera(Camera* camera) {
	camera_ = camera;
	if (object_) {
		object_->SetCamera(camera_);
	}
}

void Player::SetPosition(const Vector3& position) {
	if (object_) {
		object_->SetTranslate(position);
	}
}

void Player::SetPendingTeleport(const Vector3& position) {
	pendingTeleport_ = true;
	pendingTeleportPosition_ = position;
}

void Player::SetRidingPlatformDelta(const Vector3& delta) { ridingPlatformDelta_ = delta; }

void Player::ClearRidingPlatformDelta() { ridingPlatformDelta_ = {0.0f, 0.0f, 0.0f}; }

Vector3 Player::GetHeadbornPosition() const {
	if (!object_) {
		return { 0.0f, 0.0f, 0.0f };
	}

	auto boneWorldOpt = object_->GetBoneWorldMatrix("ボーン.004");
	if (!boneWorldOpt) {
		return object_->GetTranslate();
	}

	const Matrix4x4& world = *boneWorldOpt;

	// ボーン原点から口元までのローカル補正
	Vector3 mouthLocalOffset = { 0.0f, 0.5f, 0.0f };

	// モデル見た目合わせ用のローカルYaw補正
	const float mouthLocalYawOffsetDeg = 10.0f;
	const float mouthLocalYawOffsetRad = mouthLocalYawOffsetDeg * (3.14159265f / 180.0f);

	const float s = std::sin(mouthLocalYawOffsetRad);
	const float c = std::cos(mouthLocalYawOffsetRad);

	Vector3 rotatedOffset = {
		mouthLocalOffset.x * c - mouthLocalOffset.z * s,
		mouthLocalOffset.y,
		mouthLocalOffset.x * s + mouthLocalOffset.z * c
	};

	Vector3 position = {
		rotatedOffset.x * world.m[0][0] + rotatedOffset.y * world.m[1][0] + rotatedOffset.z * world.m[2][0] + world.m[3][0],
		rotatedOffset.x * world.m[0][1] + rotatedOffset.y * world.m[1][1] + rotatedOffset.z * world.m[2][1] + world.m[3][1],
		rotatedOffset.x * world.m[0][2] + rotatedOffset.y * world.m[1][2] + rotatedOffset.z * world.m[2][2] + world.m[3][2]
	};

	return position;
}

Vector3 Player::GetPosition() const {
	if (!object_) {
		return {0.0f, 0.0f, 0.0f};
	}
	return object_->GetTranslate();
}

Vector3 Player::GetRotate() const {
	if (!object_) {
		return {0.0f, 0.0f, 0.0f};
	}
	return object_->GetRotate();
};

CollisionUtility::OBB Player::GetPlayerOBB(const Vector3& position) const {
	Transform t;
	t.translate = position;
	t.rotate = object_ ? object_->GetRotate() : Vector3{0.0f, 0.0f, 0.0f};
	t.scale = {1.0f, 1.0f, 1.0f};

	return CollisionUtility::MakeOBBFromTransform(t, colliderHalfSize_);
}

void Player::AddWater(float amount) {
	waterGauge_ += amount;
	if (waterGauge_ > maxWaterGauge_) {
		waterGauge_ = maxWaterGauge_;
	}
	if (waterGauge_ < 0.0f) {
		waterGauge_ = 0.0f;
	}
}

bool Player::ConsumeWater(float amount) {
	if (waterGauge_ < amount) {
		return false;
	}
	waterGauge_ -= amount;
	if (waterGauge_ < 0.0f) {
		waterGauge_ = 0.0f;
	}
	return true;
}

bool Player::TryShotTongue(const Vector3& direction) {
	if (!CanStartTongueShot()) {
		return false;
	}

	if (!ConsumeWater(tongueWaterCost_)) {
		return false;
	}

	tongue_->Shot(direction);
	return true;
}

void Player::SetYawFromCamera(float cameraYaw) {
	if (!object_) {
		return;
	}

	object_->SetRotate({0.0f, cameraYaw + modelYawOffset_, 0.0f});
}

void Player::ResolveHorizontalCollisions(const Vector3& previousPosition) {
	if (!object_ || !blockColliders_) {
		return;
	}

	Vector3 position = object_->GetTranslate();
	const float kGroundEpsilon = 0.05f;

	auto isWalkableFromHit = [&](const CollisionUtility::OBB& playerObb, const CollisionUtility::OBB& block) -> bool {
		auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);
		if (!hit.hit || hit.penetration <= 0.0f) {
			return false;
		}

		// hit.normal は「プレイヤー -> ブロック」向きなので反転して、
		// プレイヤーを外へ出す向きで地面判定する
		Vector3 pushNormal = hit.normal * -1.0f;
		return IsGroundSurface(pushNormal);
	};

	if (position.x != previousPosition.x) {
		Vector3 testPos = {position.x, previousPosition.y + kGroundEpsilon, previousPosition.z};

		CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

		for (const auto& block : *blockColliders_) {
			auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);
			if (!hit.hit || hit.penetration <= 0.0f) {
				continue;
			}

			Vector3 pushNormal = hit.normal * -1.0f;

			// 坂や床は X を戻さず、後段の縦解決に任せる
			if (IsGroundSurface(pushNormal)) {
				continue;
			}

			position.x = previousPosition.x;
			break;
		}
	}

	if (position.z != previousPosition.z) {
		Vector3 testPos = {position.x, previousPosition.y + kGroundEpsilon, position.z};

		CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

		for (const auto& block : *blockColliders_) {
			auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);
			if (!hit.hit || hit.penetration <= 0.0f) {
				continue;
			}

			Vector3 pushNormal = hit.normal * -1.0f;

			// 坂や床は Z を戻さず、後段の縦解決に任せる
			if (IsGroundSurface(pushNormal)) {
				continue;
			}

			position.z = previousPosition.z;
			break;
		}
	}

	object_->SetTranslate(position);
}

void Player::ResolveVerticalCollisions(const Vector3& previousPosition) {
	(void)previousPosition;

	if (!object_) {
		return;
	}

	Vector3 position = object_->GetTranslate();
	isOnGround_ = false;

	if (!blockColliders_) {
		object_->SetTranslate(position);
		return;
	}

	for (const auto& block : *blockColliders_) {
		CollisionUtility::OBB playerObb = GetPlayerOBB(position);
		auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);

		if (!hit.hit || hit.penetration <= 0.0f) {
			continue;
		}

		const float kSkinWidth = 0.001f;

		// プレイヤーを外へ出す向き
		Vector3 pushNormal = hit.normal * -1.0f;

		if (IsGroundSurface(pushNormal)) {
			// 坂・床は横へ押さず、上方向だけ持ち上げる
			position.y += hit.penetration + kSkinWidth;

			if (velocity_.y < 0.0f) {
				velocity_.y = 0.0f;
			}
			isOnGround_ = true;
			continue;
		}

		if (IsCeilingSurface(pushNormal)) {
			// 天井は下方向だけ補正
			position.y -= hit.penetration + kSkinWidth;

			if (velocity_.y > 0.0f) {
				velocity_.y = 0.0f;
			}
			continue;
		}

		// 壁は従来どおり法線方向で押し戻す
		position += pushNormal * (hit.penetration + kSkinWidth);
	}

	object_->SetTranslate(position);
}

void Player::CheckTongueBlockHook() {
	if (!tongue_ || !blockColliders_) {
		return;
	}

	if (tongue_->GetState() != Tongue::State::Extending) {
		return;
	}

	ResetTongueHitDebug();

	Vector3 start = tongue_->GetPrevPosition();
	Vector3 end = tongue_->GetPosition();
	Vector3 delta = end - start;
	float moveLen = Length3(delta);

	CollisionUtility::Sphere baseSphere = tongue_->GetHitSphere();
	int steps = std::max(1, static_cast<int>(std::ceil(moveLen / std::max(0.05f, baseSphere.radius * 0.5f))));

	for (int s = 1; s <= steps; ++s) {
		float t = static_cast<float>(s) / static_cast<float>(steps);
		CollisionUtility::Sphere testSphere = baseSphere;
		testSphere.center = start + delta * t;

		// 【重要】壁判定の前に、エネミー判定を行う
		if (enemyManager_) {
			BaseEnemy* hitEnemy = nullptr;
			enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
				if (hitEnemy || !e)
					return;
				// フック可能な敵（センチネル・フック等）かチェック
				if (!e->IsGrappable())
					return;

				CollisionUtility::OBB enemyObb = e->GetOBB(e->GetPosition(), 0.8f);
				auto hitE = CollisionUtility::IntersectSphere_OBB_Detailed(testSphere, enemyObb);
				if (hitE.hit)
					hitEnemy = e;
			});

			if (hitEnemy) {
				// 敵に「刺さった方向」を伝える（これで敵が逃げ出す）
				Vector3 shotDir = Normalize3(delta);
				hitEnemy->OnTongueHit(shotDir);

				lastHitEnemy_ = hitEnemy; // スリングショット用にこの敵を記憶
				Vector3 hookPos = hitEnemy->GetPosition();
				tongue_->SetHooked(hookPos);
				tonguePullTarget_ = hookPos; // 敵の位置を目標にセット

				velocity_ = {0.0f, 0.0f, 0.0f};
				CancelJumpCharge();
				TransitionTo(MovementState::TonguePulling); // 引っ張り状態へ
				return;
			}
		}

		// --- 2. 既存のブロック判定（継続） ---
		for (const auto& block : *blockColliders_) {
			auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(testSphere, block);
			if (!hit.hit) {
				continue;
			}

			Vector3 rawHitNormal = Normalize3(hit.normal);
			Vector3 correctedHitNormal = ResolveHookSurfaceNormalFromPlayerCapsule(block, GetPosition(), hit.point, delta);
			Vector3 usedHitNormal = debugIgnoreHookSurfaceCorrection_ ? rawHitNormal : correctedHitNormal;

			if(IsGroundSurface(usedHitNormal)){
				tongue_->StartReturn();
				return;
			}

			SetupClingSurfaceFromHit(block, hit.point, usedHitNormal);
			UpdateClingStageObjectFromHitPoint(hit.point);

			Vector3 hookPos = hit.point + clingSurfaceNormal_ * tongueHookSurfaceOffset_;
			tongue_->SetHooked(hookPos);

			// 擬態処理 (Camouflage)
			if (abilityActive_ && currentAbility_ == Ability::Camouflage && stage_) {
				CollisionUtility::Sphere stageProbe;
				stageProbe.center = hit.point;
				stageProbe.radius = 0.05f;

				for (const auto& o : stage_->GetStageData().objects) {
					if (o.modelName.empty())
						continue;
					Transform t_obj;
					t_obj.translate = o.position;
					t_obj.rotate = o.rotation;
					t_obj.scale = o.scale;
					CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t_obj, {1.0f, 1.0f, 1.0f});
					auto hitObj = CollisionUtility::IntersectSphere_OBB_Detailed(stageProbe, obb);
					if (hitObj.hit) {
						mimicModelName_ = o.modelName;
						mimicColor_ = o.color;
						mimicColor_.w = 1.0f;
						mimicScale_ = {1.0f, 1.0f, 1.0f};
						if (object_) {
							object_->SetModel(mimicModelName_);
							object_->SetScale(mimicScale_);
						}
                    isMimicking_ = true;
					// start mimic timer (frames)
					mimicTimer_ = baseCamouflageDuration_ * camouflageDurationMultiplier_;
						abilityActive_ = false;
						break;
					}
				}
			}

			CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());
			const float kPullSkin = 0.03f;
			float playerRadiusAlongNormal = ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_);
			Vector3 clingAnchorPoint = clingSurfaceCenter_ + clingSurfaceRight_ * clingHitRightOffset_ + clingSurfaceUp_ * clingHitUpOffset_;
			tonguePullTarget_ = clingAnchorPoint + clingSurfaceNormal_ * (playerRadiusAlongNormal + kPullSkin);

			if(!IsPlayerPositionInsideMovementCylinder(tonguePullTarget_, -1.00f)){
				tongue_->StartReturn();
				hasClingSurface_ = false;
				ClearClingStageObjectTracking();
				return;
			}

			velocity_ = {0.0f, 0.0f, 0.0f};
			CancelJumpCharge();

			if (useTonguePull_) {
				TransitionTo(MovementState::TonguePulling);
			}
			return;
		}
	}
}

bool Player::CheckTongueBlockDamage() {
	if (!tongue_ || !stage_ || !blockColliders_) {
		return false;
	}

	if (tongue_->GetState() != Tongue::State::Extending) {
		return false;
	}

	const CollisionUtility::Sphere tipSphere = tongue_->GetHitSphere();
	const float hitRadius = tipSphere.radius;

	// 通常ショットは舌先だけ
	if (!tongue_->IsSweeping()) {
		for (const auto& obb : *blockColliders_) {
			if (!CollisionUtility::IntersectSphere_OBB(tipSphere, obb)) {
				continue;
			}

			stage_->ApplyDamageAtSphere(tipSphere, 1);
			tongue_->Reset();
			return true;
		}

		return false;
	}

	// 薙ぎ払い中は「口元 → 舌先」の間も当たりにする
	Vector3 mouth = tongue_->GetMouthWorldPositionPublic();
	Vector3 tip = tongue_->GetPosition();
	Vector3 segment = tip - mouth;
	float segmentLen = Length3(segment);

	int steps = std::max(1, static_cast<int>(std::ceil(segmentLen / std::max(0.05f, hitRadius * 0.75f))));
	bool damaged = false;

	for (int s = 0; s <= steps; ++s) {
		float t = static_cast<float>(s) / static_cast<float>(steps);

		CollisionUtility::Sphere sampleSphere;
		sampleSphere.radius = hitRadius;
		sampleSphere.center = mouth + segment * t;

		for (const auto& obb : *blockColliders_) {
			if (!CollisionUtility::IntersectSphere_OBB(sampleSphere, obb)) {
				continue;
			}

			stage_->ApplyDamageAtSphere(sampleSphere, 1);
			damaged = true;
		}
	}

	// 薙ぎ払いはそのまま続けたいので Reset はしない
	// ただし、このフレームは「壊した」扱いとして true を返し、
	// 後続のフック判定へ行かないようにする
	return damaged;
}

void Player::UpdateTonguePulling() {
	if (!object_ || !tongue_) {
		return;
	}

	Vector3 position = object_->GetTranslate();
	Vector3 toTarget = tonguePullTarget_ - position;
	float distance = Length3(toTarget);

	// 目標（敵）にたどり着いた瞬間の処理
	if (distance <= tonguePullEndDistance_) {

		// もし対象が敵だったら
		if (lastHitEnemy_) {
			// 【自動射出】たどり着いた瞬間に、敵の慣性を利用して自分を弾き飛ばす！
           velocity_ = lastHitEnemy_->GetVelocity();
            velocity_.y += baseJumpPowers_[0] * jumpPowerMultiplier_ * 10.0f; // 少し上に跳ね上げる (upgrade applied)

			// 重要なのは「遷移する前に情報をクリアする」こと
			lastHitEnemy_ = nullptr;
			tongue_->StartReturn();
			TransitionTo(MovementState::Jumping); // そのまま空中へ
			return;
		}

		Vector3 snapped = tonguePullTarget_;
		if (hasClingSurface_) {
			snapped = ClampPositionToCurrentClingSurface(snapped);
			ResolveCurrentClingPenetration(snapped);
		}

		object_->SetTranslate(snapped);
		velocity_ = { 0.0f, 0.0f, 0.0f };
		isOnGround_ = false;

		 tongue_->StartReturn();

		if (IsCeilingSurface(clingSurfaceNormal_)) {
			TransitionTo(MovementState::WallClinging);
		}
		else {
			TransitionTo(MovementState::WallClinging);
		}
		return;
	}

	Vector3 dir = Normalize3(toTarget);
	position += dir * tonguePullSpeed_;
	object_->SetTranslate(position);

	float yaw = std::atan2(dir.x, dir.z) + modelYawOffset_;
	object_->SetRotate({0.0f, yaw, 0.0f});
}

void Player::Update() {
	if (!object_) {
		return;
	}

	suppressTongueShotThisFrame_ = false;

	if (!abilityConfigLoaded_) {
		LoadAbilityConfig();
	}

	// 経験値とレベルアップの適用
	ApplyPendingAbilityXP();
	ApplyPendingLevelUps();

	float cameraYaw = 0.0f;
	bool isAimMode = false;
	Vector3 cameraForward = {0.0f, 0.0f, 1.0f};

	if (cameraController_) {
		cameraYaw = cameraController_->GetYaw();
		isAimMode = cameraController_->IsAimMode();
		cameraForward = cameraController_->GetForwardDirection();
	}

	if (pendingTeleport_) {
		object_->SetTranslate(pendingTeleportPosition_);
		velocity_ = {0.0f, 0.0f, 0.0f};
		TransitionTo(MovementState::Jumping);
		pendingTeleport_ = false;
	}

	if (isMimicking_ && mimicTimer_ > 0.0f) {
		mimicTimer_ -= 1.0f; // Update is per-frame
		if (mimicTimer_ <= 0.0f) {
			EndMimic();
		}
	}

	if (ridingPlatformDelta_.x != 0.0f || ridingPlatformDelta_.y != 0.0f || ridingPlatformDelta_.z != 0.0f) {
		Vector3 pos = object_->GetTranslate();
		pos.x += ridingPlatformDelta_.x;
		pos.y += ridingPlatformDelta_.y;
		pos.z += ridingPlatformDelta_.z;
		object_->SetTranslate(pos);

		const float kInvDt = 60.0f;
		velocity_.x += ridingPlatformDelta_.x * kInvDt;
		velocity_.z += ridingPlatformDelta_.z * kInvDt;
		if (std::fabs(ridingPlatformDelta_.y) > 1e-6f)
			velocity_.y = 0.0f;

		Logger::Log(
		    std::string("Player applied riding delta early posAfter:") + std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z) +
		    " delta:" + std::to_string(ridingPlatformDelta_.x) + "," + std::to_string(ridingPlatformDelta_.y) + "," + std::to_string(ridingPlatformDelta_.z) + "\n");

		ridingPlatformDelta_ = {0.0f, 0.0f, 0.0f};
	}

	// --- センチネル追従ロジック ---
	if (moveState_ == MovementState::TonguePulling) {
		// 【安全性強化】lastHitEnemy_ が有効な間だけ処理する
		if (lastHitEnemy_) {
			// 敵が逃げているので、毎フレーム最新の位置にフックを吸着させる
			tonguePullTarget_ = lastHitEnemy_->GetPosition();
			if (tongue_) {
				tongue_->SetHooked(tonguePullTarget_);
			}
		}

		// 【任意リリース】Spaceキーで加速を受け継いでジャンプ
		if (input_->IsTriggerKey(DIK_SPACE) ||
			input_->IsTriggerPad(XINPUT_GAMEPAD_A)) {
			if (lastHitEnemy_) {
				// 敵の逃走速度を自分の速度として奪う（スリングショット）
				velocity_ = lastHitEnemy_->GetVelocity();
                velocity_.y += baseJumpPowers_[0] * jumpPowerMultiplier_; // 少し上に跳ねる
				lastHitEnemy_ = nullptr;
			}
			if (tongue_)
				tongue_->StartReturn();
			TransitionTo(MovementState::Jumping);
		}
	}

	if(moveState_ == MovementState::WallClinging ||
	   moveState_ == MovementState::CeilingCrawling ||
	   moveState_ == MovementState::TonguePulling){
		RefreshMovingClingSurfaceFromStage();
	}

	switch (moveState_) {
	case MovementState::Root: {
		Vector3 previousPosition = object_->GetTranslate();

		MoveHorizontal(cameraYaw);
		ResolveHorizontalCollisions(previousPosition);
		ResolveMovementLimitCylinder();

		UpdateJumpCharge();

		Vector3 beforeVertical = object_->GetTranslate();
		ApplyGravity();
		ResolveVerticalCollisions(beforeVertical);
		ResolveMovementLimitCylinder();

		if (isOnGround_) {
			TransitionTo(MovementState::Root);
		}
		else {
			TransitionTo(MovementState::Jumping);
		}
		break;
	}

	case MovementState::Jumping: {
		Vector3 previousPosition = object_->GetTranslate();

		UpdateAirborneHorizontalMove();
		ResolveHorizontalCollisions(previousPosition);
		ResolveMovementLimitCylinder();

		Vector3 beforeVertical = object_->GetTranslate();
		ApplyGravity();
		ResolveVerticalCollisions(beforeVertical);
		ResolveMovementLimitCylinder();

		if (isOnGround_) {
			TransitionTo(MovementState::Root);
		}
		else {
			TransitionTo(MovementState::Jumping);
		}
		break;
	}

	case MovementState::WallClinging:
		UpdateWallClinging(cameraYaw);
		break;

	case MovementState::CeilingCrawling:
		UpdateCeilingCrawling();
		break;

	case MovementState::TonguePulling:
		UpdateTonguePulling();
		break;
	}

	// Apply riding platform delta after physics and collision resolution so it is
	// not overwritten by movement/collision corrections.
	if (ridingPlatformDelta_.x != 0.0f || ridingPlatformDelta_.y != 0.0f || ridingPlatformDelta_.z != 0.0f) {
		Vector3 pos = object_->GetTranslate();
		pos.x += ridingPlatformDelta_.x;
		pos.y += ridingPlatformDelta_.y;
		pos.z += ridingPlatformDelta_.z;
		object_->SetTranslate(pos);
		ResolveMovementLimitCylinder();
	}
	// エイム中はプレイヤーの正面をカメラへ合わせる
	if (isAimMode) {
		SetYawFromCamera(cameraYaw);
	}

	if(!suppressTongueShotThisFrame_ &&
	   moveState_ != MovementState::CeilingCrawling &&
	   moveState_ != MovementState::WallClinging &&
	   (input_->IsTriggerMouse(0) || input_->GetRightTrigger() >= 0.5f)) {
		Vector3 shotDirection = cameraForward;

		if (hasAimTargetPoint_ && tongue_) {
			Vector3 mouthPos = tongue_->GetMouthWorldPositionPublic();
			Vector3 toAim = aimTargetPoint_ - mouthPos;

			float lenSq = toAim.x * toAim.x + toAim.y * toAim.y + toAim.z * toAim.z;
			if (lenSq > 1e-8f) {
				float invLen = 1.0f / std::sqrt(lenSq);
				shotDirection = {toAim.x * invLen, toAim.y * invLen, toAim.z * invLen};
			}
		}

		TryShotTongue(shotDirection);
	}

	// B key: perform tongue-beam sweep (扇状薙ぎ)
	if (input_->IsTriggerKey(DIK_E) || input_->IsPressPad(XINPUT_GAMEPAD_B)) {
		// Use player's facing direction (yaw) for the sweep so arc is relative to player forward
		float yawForward = GetYaw();
		Vector3 beamDir = {std::sin(yawForward), 0.0f, std::cos(yawForward)};
		// TryUseBeam handles cooldown and water cost
		if (TryUseBeam(beamDir)) {
			// Visual: fire tongue in central direction if available
			if (tongue_) {
				// If tongue is idle, animate it sweeping for visual feedback
				if (!tongue_->IsBusy()) {
					// Use beamHalfAngle_ from Player and a slightly longer duration for a slower sweep
					tongue_->ShotSweep(beamDir, beamHalfAngleDeg_);
				}
			}
		}
	}

	if (tongue_) {
		tongue_->Update(1.0f / 60.0f);

		bool brokeBlock = CheckTongueBlockDamage();
		if (!brokeBlock) {
			CheckTongueBlockHook();
		}
	}

	// Q key: cycle selected ability
	// E key: activate camouflage (next tongue hit will mimic)
	if (input_->IsTriggerKey(DIK_Q) || input_->IsPressPad(XINPUT_GAMEPAD_X)) {
		if (isMimicking_) {
			EndMimic();
		} else {
			abilityActive_ = true;
		}
	}

	// F key: activate sonar immediately
	if (input_->IsTriggerKey(DIK_F) || input_->IsPressPad(XINPUT_GAMEPAD_Y)) {
		sonarTimer_ = sonarDuration_ * 60.0f;
	}

	// Sonar active: reveal nearby stage objects and enemies by adjusting alpha
	if (sonarTimer_ > 0.0f) {
		if (stage_) {
			for (const auto& o : stage_->GetStageData().objects) {
				float dx = o.position.x - GetPosition().x;
				float dy = o.position.y - GetPosition().y;
				float dz = o.position.z - GetPosition().z;
				float dist2 = dx * dx + dy * dy + dz * dz;
				if (dist2 <= sonarRange_ * sonarRange_) {
					Vector4 c = o.color;
					c.w = sonarAlpha_;
					stage_->SetInstanceColorById(o.id, c);
				} else {
					stage_->SetInstanceColorById(o.id, o.color);
				}
			}
		}

		// 【追加・改造】エネミーへの通知とハイライト
		if (enemyManager_) {
			enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
				if (!e)
					return; // 安全チェック（元のスタイルを維持）

				// 1. プレイヤーとエネミーの距離（XZ平面だけでなく3D距離）を測る
				Vector3 diff = e->GetPosition() - GetPosition();
				float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

				// 2. ソナーの射程内（sonarRange_ = 10m）にいる場合
				if (distSq <= sonarRange_ * sonarRange_) {
					// フェイズ・ゴーストなどに「エコーが当たった！」と通知する
					e->OnSonarHit();
					// 姿を見えるようにハイライト（アルファ値を設定）
					e->SetAlpha(sonarAlpha_);
				} else {
					// 射程外に出たエネミーは即座に元の不透明度に戻す
					e->SetAlpha(e->GetOriginalAlpha());
				}
			});
		}

		sonarTimer_ -= 1.0f; // frame decrement
		if (sonarTimer_ <= 0.0f) {
			// restore enemies alpha to their originally assigned value
			if (enemyManager_) {
				enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
					if (e)
						e->SetAlpha(e->GetOriginalAlpha());
				});
			}
			// restore stage object colors
			if (stage_) {
				for (const auto& o : stage_->GetStageData().objects) {
					stage_->SetInstanceColorById(o.id, o.color);
				}
			}
		}
	}

	// Update ability timers (frame-based)
	if (beamTimer_ > 0.0f)
		beamTimer_ = std::max(0.0f, beamTimer_ - 1.0f);

	UpdateJumpGaugeSprite();

	speedMultiplier_ = 1.0f;
}

void Player::Draw() {
	if (tongue_) {
		tongue_->Draw();
	}
	if (object_) {
		object_->Update();
		// Apply mimic or normal color while respecting currentAlpha_
		Vector4 dispColor;
		if (isMimicking_) {
			dispColor = mimicColor_;
			// 【修正】擬態中に自分がわかるように、少し青白く明滅（パルス）させる
			static float pulseTimer = 0.0f;
			pulseTimer += 1.0f / 60.0f;
			float pulse = (std::sin(pulseTimer * 10.0f) * 0.5f + 0.5f) * 0.3f;
			dispColor.x += pulse * 0.5f;
			dispColor.y += pulse;
			dispColor.z += pulse;
			dispColor.w *= currentAlpha_;
		} else {
			dispColor = originalColor_;
			dispColor.w = currentAlpha_;
		}
		object_->SetColor(dispColor);
		object_->SetDissolve(1.0f - currentAlpha_);
		object_->Draw();
	}


}

void Player::MoveHorizontal(float cameraYaw) {
	Vector3 position = object_->GetTranslate();
	lastMove_ = { 0.0f, 0.0f, 0.0f };

	if (input_->IsPushKey(DIK_R)) {
		position = { 0.0f, resetHeight_, 0.0f };
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		isOnGround_ = false;
		CancelJumpCharge();
		if (tongue_) {
			tongue_->Reset();
		}
		TransitionTo(MovementState::Jumping);
		object_->SetTranslate(position);
		return;
	}

	Vector3 inputDir = GetMoveInputDirection(cameraYaw);

	float currentYaw = object_->GetRotate().y - modelYawOffset_;

	if (LengthXZ(inputDir) > 0.0001f) {
		float desiredYaw = std::atan2(inputDir.x, inputDir.z);
		float yawDiff = WrapAnglePi(desiredYaw - currentYaw);

		float yawStep = std::clamp(yawDiff, -turnSpeedRad_, turnSpeedRad_);
		currentYaw += yawStep;

		object_->SetRotate({ 0.0f, currentYaw + modelYawOffset_, 0.0f });

		float remainYawDiff = std::abs(WrapAnglePi(desiredYaw - currentYaw));
		float moveStartAngleThresholdRad = moveStartAngleThresholdDeg_ * (3.14159265f / 180.0f);

		Vector3 targetVelocity = { 0.0f, 0.0f, 0.0f };
		if (remainYawDiff <= moveStartAngleThresholdRad) {
			float actualSpeed = moveSpeed_ * speedMultiplier_;
			targetVelocity.x = inputDir.x * actualSpeed;
			targetVelocity.z = inputDir.z * actualSpeed;
		}

		groundMoveVelocity_.x = ApproachFloat(groundMoveVelocity_.x, targetVelocity.x, groundAcceleration_);
		groundMoveVelocity_.z = ApproachFloat(groundMoveVelocity_.z, targetVelocity.z, groundAcceleration_);
	}
	else {
		groundMoveVelocity_.x = ApproachFloat(groundMoveVelocity_.x, 0.0f, groundDeceleration_);
		groundMoveVelocity_.z = ApproachFloat(groundMoveVelocity_.z, 0.0f, groundDeceleration_);
	}

	position.x += groundMoveVelocity_.x;
	position.z += groundMoveVelocity_.z;

	float moveLen = LengthXZ(groundMoveVelocity_);
	if (moveLen > 0.0001f) {
		lastMove_.x = groundMoveVelocity_.x / moveLen;
		lastMove_.z = groundMoveVelocity_.z / moveLen;
	}

	velocity_.x = groundMoveVelocity_.x;
	velocity_.z = groundMoveVelocity_.z;

	object_->SetTranslate(position);

	if (LengthXZ(groundMoveVelocity_) > 0.0001f) {
		object_->PlayAnimation("walk", false);
	}
	else {
		object_->StopAnimation(0.1f);
	}
}

void Player::UpdateJumpCharge() {
	const bool jumpTrigger =
		input_->IsTriggerKey(DIK_SPACE) ||
		input_->IsTriggerPad(XINPUT_GAMEPAD_A);

	const bool jumpPress =
		input_->IsPushKey(DIK_SPACE) ||
		input_->IsPressPad(XINPUT_GAMEPAD_A);

	if (!IsOnGround()) {
		isChargingJump_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
		isJumpChargeCanceled_ = false;
		return;
	}

	if (jumpTrigger) {
		isChargingJump_ = true;
		isJumpChargeCanceled_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
	}

	if (!isChargingJump_) {
		return;
	}

	if (jumpPress) {
		chargeTimer_ += 1.0f;

		int currentLevel = GetCurrentChargeLevel();
		int allowedLevel = GetAllowedChargeLevel();

		if (currentLevel >= allowedLevel) {
			chargeMaxHoldTimer_ += 1.0f;
			if (chargeMaxHoldTimer_ >= chargeCancelHoldLimit_) {
				CancelJumpCharge();
				return;
			}
		}
		else {
			chargeMaxHoldTimer_ = 0.0f;
		}
	}

	if (!jumpPress) {
		if (!isJumpChargeCanceled_) {
			int chargeLevel = GetCurrentChargeLevel();
			chargeLevel = std::min(chargeLevel, GetAllowedChargeLevel());
			ExecuteChargedJump(chargeLevel);
			TransitionTo(MovementState::Jumping);
		}

		isChargingJump_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
		isJumpChargeCanceled_ = false;
	}
}

int Player::GetCurrentChargeLevel() const {
	if (chargeTimer_ >= chargeThresholds_[2])
		return 3;
	if (chargeTimer_ >= chargeThresholds_[1])
		return 2;
	if (chargeTimer_ >= chargeThresholds_[0])
		return 1;
	return 0;
}

int Player::GetAllowedChargeLevel() const { return std::clamp(chargeStock_, 0, kMaxChargeLevel_); }

void Player::ExecuteChargedJump(int chargeLevel) {
	chargeLevel = std::clamp(chargeLevel, 0, kMaxChargeLevel_);

	// 【指示通り】垂直方向の減速は水平方向の1.5倍緩くする（倍率を1.5倍にする）
	// 例：水平が 0.2 (激重) のとき、垂直は 0.3 (少し跳べる) になる
	float verticalMultiplier = std::min(1.0f, speedMultiplier_ * 1.5f);

    // use configured base jump powers scaled by upgrade multiplier and any slowdown multiplier
    float baseJump = (chargeLevel >= 0 && chargeLevel < 4) ? baseJumpPowers_[chargeLevel] : baseJumpPowers_[std::clamp(chargeLevel, 0, 3)];

	lockedJumpMoveVelocity_.x = groundMoveVelocity_.x;
	lockedJumpMoveVelocity_.z = groundMoveVelocity_.z;

	velocity_.y = baseJump * jumpPowerMultiplier_ * verticalMultiplier;
	isOnGround_ = false;

	if (chargeLevel > 0) {
		// chargeStock_ -= chargeLevel;
		if (chargeStock_ < 0) {
			chargeStock_ = 0;
		}
	}
}

void Player::CancelJumpCharge() {
	isJumpChargeCanceled_ = true;
	isChargingJump_ = false;
	chargeTimer_ = 0.0f;
	chargeMaxHoldTimer_ = 0.0f;
}

void Player::ApplyGravity() {
	Vector3 position = object_->GetTranslate();

	isOnGround_ = false;

	velocity_.y += gravity_;
	position.y += velocity_.y;

	// 非常用の落下リセット
	if (position.y <= -50.0f) {
		position = {0.0f, resetHeight_, 0.0f};
		velocity_ = {0.0f, 0.0f, 0.0f};
		if (tongue_) {
			tongue_->Reset();
		}
	}

	object_->SetTranslate(position);
}

Vector3 Player::GetMoveInputDirection(float cameraYaw) const {
	Vector3 inputDir = { 0.0f, 0.0f, 0.0f };

	Vector3 forward = { std::sin(cameraYaw), 0.0f, std::cos(cameraYaw) };
	Vector3 right = { std::cos(cameraYaw), 0.0f, -std::sin(cameraYaw) };

	// キーボードのデジタル入力
	float keyX = 0.0f;
	float keyY = 0.0f;
	if (input_->IsPushKey(DIK_D)) { keyX += 1.0f; }
	if (input_->IsPushKey(DIK_A)) { keyX -= 1.0f; }
	if (input_->IsPushKey(DIK_W)) { keyY += 1.0f; }
	if (input_->IsPushKey(DIK_S)) { keyY -= 1.0f; }

	// パッドのアナログ入力
	float padX = input_->GetLeftStickX();
	float padY = input_->GetLeftStickY();

	// 両方を合成して、最後に1回だけ方向決定
	float moveX = keyX + padX;
	float moveY = keyY + padY;

	// 長さ1を超えないようにする
	float len2 = moveX * moveX + moveY * moveY;
	if (len2 > 1.0f) {
		float invLen = 1.0f / std::sqrt(len2);
		moveX *= invLen;
		moveY *= invLen;
	}

	inputDir += right * moveX;
	inputDir += forward * moveY;

	float len = LengthXZ(inputDir);
	if (len > 0.0001f) {
		inputDir.x /= len;
		inputDir.z /= len;
	}

	return inputDir;
}

void Player::UpdateAirborneHorizontalMove() {
	Vector3 position = object_->GetTranslate();

	position.x += lockedJumpMoveVelocity_.x;
	position.z += lockedJumpMoveVelocity_.z;

	velocity_.x = lockedJumpMoveVelocity_.x;
	velocity_.z = lockedJumpMoveVelocity_.z;

	object_->SetTranslate(position);
}

const char* Player::GetMovementStateName() const {
	switch (moveState_) {
	case MovementState::Root:
		return "Root";
	case MovementState::Jumping:
		return "Jumping";
	case MovementState::WallClinging:
		return "WallClinging";
	case MovementState::TonguePulling:
		return "TonguePulling";
	case MovementState::CeilingCrawling:
		return "CeilingCrawling";
	}
	return "Unknown";
}

void Player::TransitionTo(MovementState nextState) {
	if (moveState_ == nextState) {
		return;
	}

	const bool wasClingState =
		(moveState_ == MovementState::WallClinging ||
			moveState_ == MovementState::CeilingCrawling);

	const bool nextIsClingState =
		(nextState == MovementState::WallClinging ||
			nextState == MovementState::CeilingCrawling);

	// 張り付き系から離れるときだけ、通常の立ち姿勢へ戻す
	if (wasClingState && !nextIsClingState && object_) {
		float restoreYaw = object_->GetRotate().y;

		// まずは現在の水平速度の向きを優先
		const float vxzLenSq = velocity_.x * velocity_.x + velocity_.z * velocity_.z;
		if (vxzLenSq > 0.0001f) {
			restoreYaw = std::atan2(velocity_.x, velocity_.z) + modelYawOffset_;
		}
		// 速度が無ければ直前移動方向
		else {
			const float lxzLenSq = lastMove_.x * lastMove_.x + lastMove_.z * lastMove_.z;
			if (lxzLenSq > 0.0001f) {
				restoreYaw = std::atan2(lastMove_.x, lastMove_.z) + modelYawOffset_;
			}
			// それも無ければカメラ正面
			else if (cameraController_) {
				restoreYaw = cameraController_->GetYaw() + modelYawOffset_;
			}
		}

		object_->SetRotate({ 0.0f, restoreYaw, 0.0f });
	}

	moveState_ = nextState;

	switch (moveState_) {
	case MovementState::Root:
		velocity_.y = 0.0f;
		isOnGround_ = true;
		groundMoveVelocity_.x = lockedJumpMoveVelocity_.x;
		groundMoveVelocity_.z = lockedJumpMoveVelocity_.z;
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		wallClingGauge_ = maxWallClingGauge_;
		ClearClingStageObjectTracking();
		break;

	case MovementState::Jumping:
		isOnGround_ = false;
		lockedJumpMoveVelocity_.x = velocity_.x;
		lockedJumpMoveVelocity_.z = velocity_.z;
		ClearClingStageObjectTracking();
		break;

	case MovementState::WallClinging:
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		break;

	case MovementState::TonguePulling:
		break;

	case MovementState::CeilingCrawling:
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		break;
	}
}

void Player::DrawImGui() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode("Player")) {
		Vector3 position = GetPosition();
		Vector3 rotate = GetRotate();

		if (ImGui::TreeNode("Position / Velocity")) {
			ImGui::Text("World Position");
			ImGui::Separator();
			ImGui::Text("X : %.3f", position.x);
			ImGui::Text("Y : %.3f", position.y);
			ImGui::Text("Z : %.3f", position.z);

			ImGui::Separator();
			ImGui::Text("X : %.3f", rotate.x);
			ImGui::Text("Y : %.3f", rotate.y);
			ImGui::Text("Z : %.3f", rotate.z);

			ImGui::Separator();
			ImGui::Text("Velocity");
			ImGui::Text("VX : %.3f", velocity_.x);
			ImGui::Text("VY : %.3f", velocity_.y);
			ImGui::Text("VZ : %.3f", velocity_.z);

			ImGui::Separator();
			ImGui::Text("OnGround : %s", isOnGround_ ? "true" : "false");
			ImGui::Text("Collider Half : %.2f %.2f %.2f", colliderHalfSize_.x, colliderHalfSize_.y, colliderHalfSize_.z);
			ImGui::Text("BlockCollider Count : %d", blockColliders_ ? static_cast<int>(blockColliders_->size()) : 0);
			ImGui::TreePop();

     // 能力表示用の UI
		if (ImGui::TreeNode("Abilities")) {
			if (abilityStates_.empty()) {
				ImGui::Text("No abilities yet.");
			} else {
				for (const auto &kv : abilityStates_) {
					AbilityId id = kv.first;
					const AbilityState &s = kv.second;

					float need = XPToNextLevel(s.level);
					float progress = (need > 0.0f) ? (s.xp / need) : 0.0f;
					if (progress < 0.0f) progress = 0.0f;
					if (progress > 1.0f) progress = 1.0f;

					char buf[128];
					snprintf(buf, sizeof(buf), "%s - L%d", AbilityIdToString(id), s.level);
					ImGui::Text("%s", buf);
					ImGui::SameLine();
					char pct[64];
					snprintf(pct, sizeof(pct), "%.0f/%.0f", s.xp, need);
					ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), pct);
				}
			}

			ImGui::Separator();
			ImGui::Text("Pending XP:");
			if (pendingAbilityXP_.empty()) {
				ImGui::Text("(none)");
			} else {
				for (const auto &p : pendingAbilityXP_) {
					ImGui::Text("%s : %.1f", AbilityIdToString(p.first), p.second);
				}
			}

			ImGui::Separator();
			ImGui::Text("Pending LevelUps:");
			if (pendingLevelUps_.empty()) {
				ImGui::Text("(none)");
			} else {
				for (const auto &lv : pendingLevelUps_) {
					ImGui::Text("%s -> L%d", AbilityIdToString(lv.first), lv.second);
				}
			}

      // 手動XP付与（デバッグ）
			ImGui::Separator();
			ImGui::Text("Manual XP Giver (debug)");
			static int selIdx = 0;
			static const char* abilityNames[] = { "JumpPower", "TongueRange", "SonarDuration", "WallClingDuration", "CamouflageDuration" };
			if (ImGui::Combo("Ability", &selIdx, abilityNames, IM_ARRAYSIZE(abilityNames))) {
				// selection changed
			}
			static float giveXPAmount = 10.0f;
			ImGui::InputFloat("XP Amount", &giveXPAmount, 1.0f, 10.0f, "%.1f");
			ImGui::SameLine();
			if (ImGui::Button("Enqueue XP")) {
				AbilityId id = static_cast<AbilityId>(selIdx + 1); // enum starts at 1 (Unknown=0)
				EnqueueAbilityXP(id, giveXPAmount);
			}
			ImGui::SameLine();
			if (ImGui::Button("Give Now")) {
				AbilityId id = static_cast<AbilityId>(selIdx + 1);
                // 即時適用：enqueueしてから保留リストを処理する
				EnqueueAbilityXP(id, giveXPAmount);
				ApplyPendingAbilityXP();
				ApplyPendingLevelUps();
			}

			ImGui::TreePop();
		}
	}

    // 能力設定エディタ（ランタイムで調整可能、永続化しない）
		if (ImGui::TreeNode("Ability Config Editor")) {
			bool anyChanged = false;
            // 既知の能力一覧
			static const AbilityId allAbilities[] = { AbilityId::JumpPower, AbilityId::TongueRange, AbilityId::SonarDuration, AbilityId::WallClingDuration, AbilityId::CamouflageDuration };
			for (AbilityId id : allAbilities) {
				char label[64];
				snprintf(label, sizeof(label), "%s_enabled", AbilityIdToString(id));
				bool enabled = abilityStates_.find(id) != abilityStates_.end();
				if (ImGui::Checkbox(label, &enabled)) {
					anyChanged = true;
					if (enabled) {
						abilityStates_[id] = AbilityState();
					} else {
						abilityStates_.erase(id);
					}
				}
				ImGui::SameLine();
				ImGui::Text("%s", AbilityIdToString(id));

                // 各能力の設定項目を表示
				if (id == AbilityId::JumpPower) {
					// base jump powers
					for (int i = 0; i < 4; ++i) {
						char buf[64];
						snprintf(buf, sizeof(buf), "Jump Base[%d]", i);
						if (ImGui::InputFloat(buf, &baseJumpPowers_[i], 0.0f, 0.0f, "%.3f")) anyChanged = true;
					}
					if (ImGui::InputFloat("Jump perLevel", &jumpPowerPerLevel_, 0.0f, 0.0f, "%.3f")) anyChanged = true;
				} else if (id == AbilityId::TongueRange) {
					if (ImGui::InputFloat("Tongue Base Distance", &baseTongueMaxDistance_, 0.0f, 0.0f, "%.2f")) anyChanged = true;
					if (ImGui::InputFloat("Tongue perLevel", &tongueRangePerLevel_, 0.0f, 0.0f, "%.3f")) anyChanged = true;
				} else if (id == AbilityId::SonarDuration) {
					if (ImGui::InputFloat("Sonar Base (s)", &baseSonarDuration_, 0.0f, 0.0f, "%.2f")) anyChanged = true;
					if (ImGui::InputFloat("Sonar perLevel", &sonarDurationPerLevel_, 0.0f, 0.0f, "%.3f")) anyChanged = true;
				} else if (id == AbilityId::WallClingDuration) {
					if (ImGui::InputFloat("WallCling Base", &baseWallClingGauge_, 0.0f, 0.0f, "%.1f")) anyChanged = true;
					if (ImGui::InputFloat("WallCling perLevel", &wallClingPerLevel_, 0.0f, 0.0f, "%.3f")) anyChanged = true;
				} else if (id == AbilityId::CamouflageDuration) {
					if (ImGui::InputFloat("Camouflage Base (frames)", &baseCamouflageDuration_, 0.0f, 0.0f, "%.0f")) anyChanged = true;
					if (ImGui::InputFloat("Camouflage perLevel", &camouflagePerLevel_, 0.0f, 0.0f, "%.3f")) anyChanged = true;
				}
				ImGui::Separator();
			}

            // 常に Apply Config を表示し、デザイナーがいつでも編集を反映できるようにする
			if (ImGui::Button("Apply Config")) {
				// apply config to runtime values based on current levels
				for (const auto &kv : abilityStates_) {
					AbilityId id = kv.first;
					int level = kv.second.level;
					float mult = 1.0f + ((id == AbilityId::JumpPower) ? jumpPowerPerLevel_ : (id == AbilityId::TongueRange) ? tongueRangePerLevel_ : (id == AbilityId::SonarDuration) ? sonarDurationPerLevel_ : (id == AbilityId::WallClingDuration) ? wallClingPerLevel_ : camouflagePerLevel_) * static_cast<float>(level - 1);
					switch (id) {
					case AbilityId::JumpPower: jumpPowerMultiplier_ = mult; break;
					case AbilityId::TongueRange: tongueRangeMultiplier_ = mult; if (tongue_) tongue_->SetMaxDistance(baseTongueMaxDistance_ * tongueRangeMultiplier_); break;
					case AbilityId::SonarDuration: sonarDurationMultiplier_ = mult; sonarDuration_ = baseSonarDuration_ * sonarDurationMultiplier_; break;
					case AbilityId::WallClingDuration: wallClingDurationMultiplier_ = mult; maxWallClingGauge_ = baseWallClingGauge_ * wallClingDurationMultiplier_; if (wallClingGauge_ > maxWallClingGauge_) wallClingGauge_ = maxWallClingGauge_; break;
					case AbilityId::CamouflageDuration: camouflageDurationMultiplier_ = mult; break;
					default: break;
					}
				}
			}
			ImGui::TreePop();
		}

        // 実効（強化後）の値（参照専用）
		if (ImGui::TreeNode("Ability Effective Values")) {
			// Jump power (show base per-charge * multiplier)
			ImGui::Text("Jump Power (per charge):");
			for (int i = 0; i < 4; ++i) {
				float base = baseJumpPowers_[i];
				float effective = base * jumpPowerMultiplier_;
				ImGui::Text("  Charge %d: base %.3f -> effective %.3f", i, base, effective);
			}
			// Tongue range
			float tongueEffective = baseTongueMaxDistance_ * tongueRangeMultiplier_;
			if (tongue_) tongueEffective = tongue_->GetMaxDistance();
			ImGui::Text("Tongue Range: base %.2f -> effective %.2f", baseTongueMaxDistance_, tongueEffective);

			// Sonar duration
			float sonarEffective = baseSonarDuration_ * sonarDurationMultiplier_;
			ImGui::Text("Sonar Duration: base %.2f s -> effective %.2f s", baseSonarDuration_, sonarEffective);

			// Wall cling gauge / duration
			float wallEffective = baseWallClingGauge_ * wallClingDurationMultiplier_;
			ImGui::Text("Wall Cling Gauge: base %.1f -> effective %.1f", baseWallClingGauge_, wallEffective);

			// Camouflage duration
			float camoEffective = baseCamouflageDuration_ * camouflageDurationMultiplier_;
			ImGui::Text("Camouflage Duration: base %.0f frames -> effective %.0f frames", baseCamouflageDuration_, camoEffective);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Water")) {
			ImGui::Text("Water Gauge : %.1f / %.1f", waterGauge_, maxWaterGauge_);
			ImGui::ProgressBar(maxWaterGauge_ > 0.0f ? (waterGauge_ / maxWaterGauge_) : 0.0f, ImVec2(240.0f, 22.0f));
			ImGui::Text("Tongue Cost : %.1f", tongueWaterCost_);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("State")) {
			ImGui::Text("Current State : %s", GetMovementStateName());
			ImGui::Separator();

			if (ImGui::Button("To Root")) {
				TransitionTo(MovementState::Root);
			}
			ImGui::SameLine();
			if (ImGui::Button("To Jumping")) {
				TransitionTo(MovementState::Jumping);
			}
			ImGui::SameLine();
			if (ImGui::Button("To WallClinging")) {
				TransitionTo(MovementState::WallClinging);
			}
			ImGui::SameLine();
			if (ImGui::Button("To TonguePulling")) {
				TransitionTo(MovementState::TonguePulling);
			}

			ImGui::Separator();
			ImGui::Text("Wall Gauge");
			ImGui::ProgressBar(maxWallClingGauge_ > 0.0f ? (wallClingGauge_ / maxWallClingGauge_) : 0.0f, ImVec2(240.0f, 22.0f));
			ImGui::Text("%.1f / %.1f", wallClingGauge_, maxWallClingGauge_);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Jump")) {
			ImGui::Text("Space : Hold to Charge Jump");
			ImGui::Separator();

			int stock = GetChargeStock();
			int phase = GetCurrentChargePhase();
			float phaseRate = GetCurrentChargePhaseRate();
			int visibleLevel = GetCurrentVisibleChargeLevel();

			ImGui::Text("Charge Stock : %d", stock);
			ImGui::Text("Current Level : %d", visibleLevel);

			int editableStock = stock;
			if (ImGui::SliderInt("Edit Charge Stock", &editableStock, 0, kMaxChargeLevel_)) {
				SetChargeStock(editableStock);
				stock = editableStock;
			}

			if (ImGui::Button("+1 Stock")) {
				AddChargeStock(1);
			}
			ImGui::SameLine();
			if (ImGui::Button("-1 Stock")) {
				AddChargeStock(-1);
			}
			ImGui::SameLine();
			if (ImGui::Button("Max Stock")) {
				SetChargeStock(kMaxChargeLevel_);
			}

			ImGui::Separator();

			if (stock <= 0) {
				ImGui::Text("Phase : Normal Jump Only");
			} else {
				if (IsChargeAtMaxPhase()) {
					ImGui::Text("Phase : MAX (%d / %d)", visibleLevel, stock);
				} else {
					ImGui::Text("Phase : %d / %d", phase + 1, stock);
				}
			}

			ImGui::ProgressBar(phaseRate, ImVec2(240.0f, 22.0f));
			ImGui::Text("Phase Charge : %d%%", static_cast<int>(phaseRate * 100.0f));

			if (IsChargeAtMaxPhase()) {
				ImGui::Text("Holding too long will cancel jump");
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Tongue")) {
			if (tongue_) {
				Vector3 tonguePos = tongue_->GetPosition();
				ImGui::Text("Position : %.3f %.3f %.3f", tonguePos.x, tonguePos.y, tonguePos.z);

				const char* stateName = "Idle";
				switch (tongue_->GetState()) {
				case Tongue::State::Idle:
					stateName = "Idle";
					break;
				case Tongue::State::Extending:
					stateName = "Extending";
					break;
				case Tongue::State::Hooked:
					stateName = "Hooked";
					break;
				case Tongue::State::Returning:
					stateName = "Returning";
					break;
				}

				ImGui::Text("State : %s", stateName);
				ImGui::Text("Shot : Right Click Release");
				ImGui::Text("Pull Target : %.3f %.3f %.3f", tonguePullTarget_.x, tonguePullTarget_.y, tonguePullTarget_.z);

				if (ImGui::Button("Shot Tongue")) {
					Vector3 debugDirection = {0.0f, 0.0f, 1.0f};
					if (cameraController_) {
						debugDirection = cameraController_->GetForwardDirection();
					}

					TryShotTongue(debugDirection);
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset Tongue")) {
					tongue_->Reset();
					if (moveState_ == MovementState::TonguePulling) {
						TransitionTo(MovementState::Jumping);
					}
				}
				ImGui::Checkbox("Use Tongue Pull", &useTonguePull_);

				ImGui::Separator();
				ImGui::Text("Tongue Hit Debug");
				ImGui::Checkbox("Show Raw Hit Debug", &debugShowRawTongueHit_);
				ImGui::Checkbox("Ignore Surface Correction", &debugIgnoreHookSurfaceCorrection_);
				ImGui::Checkbox("Ignore Ground Reject", &debugIgnoreGroundRejectOnRawHit_);

				if (ImGui::Button("Clear Hit Debug")) {
					ResetTongueHitDebug();
				}

				if (debugShowRawTongueHit_) {
					ImGui::Separator();
					ImGui::Text("Has Hit Debug : %s", hasTongueHitDebug_ ? "true" : "false");
					ImGui::Text("Hit Step : %d", debugTongueHitStep_);

					ImGui::Text("Hit Point : %.3f %.3f %.3f", debugTongueHitPoint_.x, debugTongueHitPoint_.y, debugTongueHitPoint_.z);

					ImGui::Text("Raw Normal : %.3f %.3f %.3f", debugTongueRawNormal_.x, debugTongueRawNormal_.y, debugTongueRawNormal_.z);
					ImGui::Text("Raw Face : %s", debugTongueRawFaceName_);

					ImGui::Text("Used Normal : %.3f %.3f %.3f", debugTongueUsedNormal_.x, debugTongueUsedNormal_.y, debugTongueUsedNormal_.z);
					ImGui::Text("Used Face : %s", debugTongueUsedFaceName_);
				}
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
#endif
}
float Player::GetJumpChargeRate() const {
	int allowedLevel = GetAllowedChargeLevel();
	if (allowedLevel <= 0)
		return 0.0f;

	float maxTime = (allowedLevel == 1) ? chargeThresholds_[0] : (allowedLevel == 2) ? chargeThresholds_[1] : chargeThresholds_[2];

	float t = chargeTimer_ / maxTime;
	return std::clamp(t, 0.0f, 1.0f);
}

int Player::GetCurrentVisibleChargeLevel() const { return std::min(GetCurrentChargeLevel(), GetAllowedChargeLevel()); }

void Player::AddChargeStock(int amount) {
	chargeStock_ += amount;
	chargeStock_ = std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::SetChargeStock(int stock) { chargeStock_ = std::clamp(stock, 0, kMaxChargeLevel_); }

int Player::GetCurrentChargePhase() const {
	int allowedLevel = GetAllowedChargeLevel();
	if (allowedLevel <= 0)
		return 0;

	if (chargeTimer_ < chargeThresholds_[0])
		return 0;
	if (allowedLevel >= 2 && chargeTimer_ < chargeThresholds_[1])
		return 1;
	if (allowedLevel >= 3 && chargeTimer_ < chargeThresholds_[2])
		return 2;
	return allowedLevel;
}

float Player::GetCurrentChargePhaseRate() const {
	int allowedLevel = GetAllowedChargeLevel();
	if (allowedLevel <= 0)
		return 0.0f;

	float start = 0.0f;
	float end = chargeThresholds_[0];
	int phase = GetCurrentChargePhase();

	if (phase <= 0) {
		start = 0.0f;
		end = chargeThresholds_[0];
	} else if (phase == 1) {
		start = chargeThresholds_[0];
		end = chargeThresholds_[1];
	} else if (phase == 2) {
		start = chargeThresholds_[1];
		end = chargeThresholds_[2];
	} else {
		return 1.0f;
	}

	float length = end - start;
	if (length <= 0.0f) {
		return 1.0f;
	}

	float t = (chargeTimer_ - start) / length;
	return std::clamp(t, 0.0f, 1.0f);
}

bool Player::IsChargeAtMaxPhase() const { return GetCurrentVisibleChargeLevel() >= GetAllowedChargeLevel() && GetAllowedChargeLevel() > 0 && GetCurrentChargePhaseRate() >= 1.0f; }

float Player::GetYaw() const {
	if (!object_) {
		return 0.0f;
	}
	return object_->GetRotate().y;
}

void Player::UpdateWallClinging(float cameraYaw) {
	(void)cameraYaw;

	Vector3 position = object_->GetTranslate();

	if (!hasClingSurface_) {
		TransitionTo(MovementState::Jumping);
		return;
	}

	wallClingGauge_ -= wallClingConsumption_;
	if (wallClingGauge_ <= 0.0f) {
		wallClingGauge_ = 0.0f;
		hasClingSurface_ = false;
		TransitionTo(MovementState::Jumping);
		return;
	}

	Vector3 moveVec = { 0.0f, 0.0f, 0.0f };

	// キーボード
	float keyX = 0.0f;
	float keyY = 0.0f;
	if (input_->IsPushKey(DIK_D)) { keyX += 1.0f; }
	if (input_->IsPushKey(DIK_A)) { keyX -= 1.0f; }
	if (input_->IsPushKey(DIK_W)) { keyY += 1.0f; }
	if (input_->IsPushKey(DIK_S)) { keyY -= 1.0f; }

	// パッド
	float padX = input_->GetLeftStickX();
	float padY = input_->GetLeftStickY();

	float moveX = keyX + padX;
	float moveY = keyY + padY;

	float len2 = moveX * moveX + moveY * moveY;
	if (len2 > 1.0f) {
		float invLen = 1.0f / std::sqrt(len2);
		moveX *= invLen;
		moveY *= invLen;
	}

	moveVec += clingSurfaceUp_ * moveY;
	moveVec += wallRightVec_ * (-moveX);

	float len = Length3(moveVec);
	if (len > 0.0001f) {
		moveVec *= (1.0f / len);
		position += moveVec * wallMoveSpeed_;
	}

	// 先に張り付いた面へ向きをそろえる
	ApplyClingSurfaceRotation();

	// まず張り付き面の範囲に拘束
	position = ClampPositionToCurrentClingSurface(position);

	// 張り付いている元の面へのめり込みを解消
	ResolveCurrentClingPenetration(position);

	// 他のブロックへのめり込みも解消
	ResolveWallClingBlockCollisions(position);

	// 押し戻し後にもう一度面へそろえる
	position = ClampPositionToCurrentClingSurface(position);

	// 面の外へ出たら壁のぼり解除
	if (!IsInsideCurrentClingSurface(position)) {
		hasClingSurface_ = false;
		object_->SetTranslate(position);
		TransitionTo(MovementState::Jumping);
		return;
	}

	if (input_->IsTriggerKey(DIK_SPACE) ||
		input_->IsPressPad(XINPUT_GAMEPAD_A)) {
		object_->SetTranslate(position);

		// 天井に張り付いているときだけ高速移動モードへ移行
		if (IsCeilingSurface(clingSurfaceNormal_)) {
			TransitionTo(MovementState::CeilingCrawling);
			return;
		}

		// 壁のときは従来どおり離脱ジャンプ
		velocity_ = clingSurfaceNormal_ * -0.15f;
        velocity_.y = baseJumpPowers_[0] * jumpPowerMultiplier_;
		hasClingSurface_ = false;
		TransitionTo(MovementState::Jumping);
		return;
	}

	// 円柱外に出ないように補正
	position = ClampPlayerPositionInsideMovementCylinder(position, -1.00f);

	object_->SetTranslate(position);
}

void Player::UpdateCeilingCrawling() {
	if (!object_ || !hasClingSurface_) {
		TransitionTo(MovementState::Jumping);
		return;
	}

	wallClingGauge_ -= wallClingConsumption_;
	if(wallClingGauge_ <= 0.0f){
		wallClingGauge_ = 0.0f;
		hasClingSurface_ = false;
		TransitionTo(MovementState::Jumping);
		return;
	}

	Vector3 position = object_->GetTranslate();

	// カメラ前方を現在の張り付き面へ射影して進行方向を作る
	Vector3 moveDir = clingSurfaceUp_;
	if (cameraController_) {
		Vector3 cameraForward = cameraController_->GetForwardDirection();
		float dot = Dot3(cameraForward, clingSurfaceNormal_);
		moveDir = cameraForward - clingSurfaceNormal_ * dot;
	}

	float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);

	if (len > 0.0001f) {
		moveDir = moveDir * (1.0f / len);
	}

	Vector3 nextPosition = position + moveDir * ceilingCrawlSpeed_;

	// 端に出たかどうかは clamp 前の位置で判定する
	if (!IsInsideCurrentClingSurface(nextPosition)) {
		Vector3 toNext = nextPosition - clingSurfaceCenter_;
		float rightDist = Dot3(toNext, clingSurfaceRight_);
		float upDist = Dot3(toNext, clingSurfaceUp_);

		Vector3 nextNormal = clingSurfaceNormal_;
		bool canTransition = false;

		// 天井面から同じブロックの側面へ移る
		if (IsCeilingSurface(clingSurfaceNormal_)) {
			if (rightDist > clingSurfaceHalfWidth_) {
				nextNormal = clingSurfaceRight_;
				canTransition = true;
			} else if (rightDist < -clingSurfaceHalfWidth_) {
				nextNormal = clingSurfaceRight_ * -1.0f;
				canTransition = true;
			} else if (upDist > clingSurfaceHalfHeight_) {
				nextNormal = clingSurfaceUp_;
				canTransition = true;
			} else if (upDist < -clingSurfaceHalfHeight_) {
				nextNormal = clingSurfaceUp_ * -1.0f;
				canTransition = true;
			}
		}

		if(canTransition){
			SetupClingSurfaceFromHit(clingBlockObb_, nextPosition, nextNormal);

			Vector3 reattachPos = ClampPositionToCurrentClingSurface(nextPosition);
			ResolveCurrentClingPenetration(reattachPos);
			ResolveWallClingBlockCollisions(reattachPos);
			reattachPos = ClampPositionToCurrentClingSurface(reattachPos);
			reattachPos = ClampPlayerPositionInsideMovementCylinder(reattachPos, -1.00f);

			object_->SetTranslate(reattachPos);

			ApplyClingSurfaceRotation();

			// 面遷移後の新しい張り付き面から、舌のフック位置を更新
			RefreshClingAnchorFromCurrentSurface();

			TransitionTo(MovementState::WallClinging);
			return;
		}

		hasClingSurface_ = false;
		object_->SetTranslate(nextPosition);
		TransitionTo(MovementState::Jumping);
		return;
	}

	// 面内なら通常の天井移動
	position = ClampPositionToCurrentClingSurface(nextPosition);
	ResolveCurrentClingPenetration(position);
	ResolveWallClingBlockCollisions(position);
	position = ClampPositionToCurrentClingSurface(position);

	position = ClampPlayerPositionInsideMovementCylinder(position, -0.40f);

	object_->SetTranslate(position);

	// 高速這い中は、カメラ前方を面へ射影した向きを前にする
	Vector3 facingDir = clingSurfaceUp_;
	if (cameraController_) {
		facingDir = cameraController_->GetForwardDirection();
	}

	ApplyClingSurfaceRotationFacing(facingDir);
	object_->PlayAnimation("walk", false);

	// 左クリックで手を離す
	if (input_->IsTriggerMouse(0) || input_->GetRightTrigger() >= 0.5f) {
		hasClingSurface_ = false;
		velocity_ = {0.0f, 0.0f, 0.0f};
		suppressTongueShotThisFrame_ = true;
		TransitionTo(MovementState::Jumping);
		return;
	}

	// もう一度 Space を押したら通常の張り付き状態へ戻す
	if (input_->IsTriggerKey(DIK_SPACE) || input_->IsTriggerPad(XINPUT_GAMEPAD_A)) {
		TransitionTo(MovementState::WallClinging);
		return;
	}
}

void Player::UpdateTransparencyByCamera(const Vector3& cameraPosition) {
	if (!object_) {
		return;
	}

	Vector3 toPlayer = {GetPosition().x - cameraPosition.x, GetPosition().y - cameraPosition.y, GetPosition().z - cameraPosition.z};

	float distance = toPlayer.Length();

	if (distance >= fadeStartDistance_) {
		currentAlpha_ = 1.0f;
	} else if (distance <= fadeEndDistance_) {
		currentAlpha_ = minAlpha_;
	} else {
		float t = (distance - fadeEndDistance_) / (fadeStartDistance_ - fadeEndDistance_);
		currentAlpha_ = minAlpha_ + (1.0f - minAlpha_) * t;
	}

	if (tongue_) {
		if (tongue_->IsBusy()) {
			tongue_->SetAlpha(1.0f);
		} else {
			tongue_->SetAlpha(currentAlpha_);
		}
	}
}

void Player::SetupClingSurfaceFromHit(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& hitNormal) {
	clingBlockObb_ = block;
	clingSurfaceNormal_ = Normalize3(hitNormal);

	// どの面に当たったかを、法線に最も近いブロック軸で決める
	int normalAxisIndex = 0;
	float bestDot = AbsDot3(block.axis[0], clingSurfaceNormal_);

	for (int i = 1; i < 3; ++i) {
		float d = AbsDot3(block.axis[i], clingSurfaceNormal_);
		if (d > bestDot) {
			bestDot = d;
			normalAxisIndex = i;
		}
	}

	Vector3 faceNormal = block.axis[normalAxisIndex];
	if (Dot3(faceNormal, clingSurfaceNormal_) < 0.0f) {
		faceNormal = faceNormal * -1.0f;
	}
	clingSurfaceNormal_ = faceNormal;

	// 面の中心
	clingSurfaceCenter_ = block.center + clingSurfaceNormal_ * block.halfLength[normalAxisIndex];

	// 面上の2軸を決める
	int axisA = (normalAxisIndex + 1) % 3;
	int axisB = (normalAxisIndex + 2) % 3;

	// 上方向に近いほうを面の縦方向にする
	Vector3 worldUp = {0.0f, 1.0f, 0.0f};
	float dotA = AbsDot3(block.axis[axisA], worldUp);
	float dotB = AbsDot3(block.axis[axisB], worldUp);

	int upAxis = axisA;
	int rightAxis = axisB;
	if (dotB > dotA) {
		upAxis = axisB;
		rightAxis = axisA;
	}

	clingSurfaceUp_ = block.axis[upAxis];
	if (Dot3(clingSurfaceUp_, worldUp) < 0.0f) {
		clingSurfaceUp_ = clingSurfaceUp_ * -1.0f;
	}

	clingSurfaceRight_ = block.axis[rightAxis];

	// 右方向は right × up = normal になる向きへ揃える
	Vector3 cross = Cross3(clingSurfaceRight_, clingSurfaceUp_);
	if (Dot3(cross, clingSurfaceNormal_) < 0.0f) {
		clingSurfaceRight_ = clingSurfaceRight_ * -1.0f;
	}

	clingSurfaceHalfWidth_ = block.halfLength[rightAxis];
	clingSurfaceHalfHeight_ = block.halfLength[upAxis];

	// 既存の壁移動ベクトルにも反映
	wallRightVec_ = clingSurfaceRight_;
	tongueHookNormal_ = clingSurfaceNormal_;
	hasClingSurface_ = true;

	// 面の中の「どこに当たったか」を保存
	Vector3 toHit = hitPoint - clingSurfaceCenter_;
	clingHitRightOffset_ = Dot3(toHit, clingSurfaceRight_);
	clingHitUpOffset_ = Dot3(toHit, clingSurfaceUp_);
}

bool Player::IsInsideCurrentClingSurface(const Vector3& position) const {
	if (!hasClingSurface_) {
		return false;
	}

	Vector3 toPos = position - clingSurfaceCenter_;
	float rightDist = Dot3(toPos, clingSurfaceRight_);
	float upDist = Dot3(toPos, clingSurfaceUp_);

	return std::abs(rightDist) <= (clingSurfaceHalfWidth_ + wallDetachMargin_) && std::abs(upDist) <= (clingSurfaceHalfHeight_ + wallDetachMargin_);
}

Vector3 Player::ClampPositionToCurrentClingSurface(const Vector3& position) const {
	if (!hasClingSurface_) {
		return position;
	}

	Vector3 toPos = position - clingSurfaceCenter_;

	float rightDist = Dot3(toPos, clingSurfaceRight_);
	float upDist = Dot3(toPos, clingSurfaceUp_);

	rightDist = ClampFloat(rightDist, -clingSurfaceHalfWidth_, clingSurfaceHalfWidth_);
	upDist = ClampFloat(upDist, -clingSurfaceHalfHeight_, clingSurfaceHalfHeight_);

	CollisionUtility::OBB playerObb = GetPlayerOBB(position);
	float pushOut = ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_) + wallKeepDistance_;

	return clingSurfaceCenter_ + clingSurfaceRight_ * rightDist + clingSurfaceUp_ * upDist + clingSurfaceNormal_ * pushOut;
}

void Player::ResolveCurrentClingPenetration(Vector3& position) const {
	if (!hasClingSurface_) {
		return;
	}

	CollisionUtility::OBB playerObb = GetPlayerOBB(position);
	auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, clingBlockObb_);

	if (!hit.hit) {
		return;
	}

	Vector3 pushNormal = hit.normal;

	// 壁の外向きにそろえる
	if (Dot3(pushNormal, clingSurfaceNormal_) < 0.0f) {
		pushNormal = pushNormal * -1.0f;
	}

	const float kSkin = 0.001f;

	// 外側へ押し出す
	position += pushNormal * (hit.penetration + kSkin);
}

void Player::ResolveWallClingBlockCollisions(Vector3& position) const {
	if (!blockColliders_) {
		return;
	}

	// 2〜3回まわして、複数ブロックへの重なりを少し安定させる
	for (int iteration = 0; iteration < 3; ++iteration) {
		bool anyHit = false;

		CollisionUtility::OBB playerObb = GetPlayerOBB(position);

		for (const auto& block : *blockColliders_) {
			auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);
			if (!hit.hit || hit.penetration <= 0.0f) {
				continue;
			}

			Vector3 pushNormal = hit.normal;

			// プレイヤー -> ブロック の法線なので、プレイヤーを外へ出す向きへ反転
			pushNormal = pushNormal * -1.0f;

			const float kSkin = 0.001f;
			position += pushNormal * (hit.penetration + kSkin);

			playerObb = GetPlayerOBB(position);
			anyHit = true;
		}

		if (!anyHit) {
			break;
		}
	}
}

bool Player::TryReattachToAdjacentSurface(const Vector3& fromPosition, const Vector3& moveDir, Vector3& outPosition) {
	if (!blockColliders_) {
		return false;
	}

	// 進行方向へ少し先の位置を基準に、近い面を探す
	Vector3 probePos = fromPosition + moveDir * clingReattachSearchDistance_;

	float bestScore = -1.0e9f;
	bool found = false;
	CollisionUtility::OBB bestBlock = {};
	Vector3 bestNormal = {0.0f, 0.0f, 0.0f};

	for (const auto& block : *blockColliders_) {
		CollisionUtility::OBB probeObb = GetPlayerOBB(probePos);
		auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(probeObb, block);
		if (!hit.hit) {
			continue;
		}

		Vector3 hitNormal = hit.normal * -1.0f;

		// 地面は除外
		if (IsGroundSurface(hitNormal)) {
			continue;
		}

		// 今いる面とほぼ同じ面も除外
		if (Dot3(hitNormal, clingSurfaceNormal_) > 0.95f) {
			continue;
		}

		// 進行方向に対して自然につながる面を優先
		float score = Dot3(hitNormal, clingSurfaceNormal_ * -1.0f);

		if (score > bestScore) {
			bestScore = score;
			bestBlock = block;
			bestNormal = hitNormal;
			found = true;
		}
	}

	if (!found) {
		return false;
	}

	SetupClingSurfaceFromHit(bestBlock, probePos, bestNormal);

	outPosition = ClampPositionToCurrentClingSurface(probePos);
	ResolveCurrentClingPenetration(outPosition);
	ResolveWallClingBlockCollisions(outPosition);
	outPosition = ClampPositionToCurrentClingSurface(outPosition);

	return true;
}

const char* Player::DebugFaceNameFromNormal(const CollisionUtility::OBB& block, const Vector3& normal) const {
	Vector3 n = Normalize3(normal);

	struct FaceInfo {
		Vector3 normal;
		const char* name;
	};

	FaceInfo faces[6] = {
	    {Normalize3(block.axis[0]),      "+Axis0"},
        {Normalize3(block.axis[0]) * -1, "-Axis0"},
        {Normalize3(block.axis[1]),      "+Axis1"},
	    {Normalize3(block.axis[1]) * -1, "-Axis1"},
        {Normalize3(block.axis[2]),      "+Axis2"},
        {Normalize3(block.axis[2]) * -1, "-Axis2"},
	};

	float bestDot = -1.0f;
	const char* bestName = "Unknown";

	for (const auto& face : faces) {
		float d = Dot3(n, face.normal);
		if (d > bestDot) {
			bestDot = d;
			bestName = face.name;
		}
	}

	return bestName;
}

void Player::ResetTongueHitDebug() {
	hasTongueHitDebug_ = false;
	debugTongueHitStep_ = -1;
	debugTongueHitPoint_ = {0.0f, 0.0f, 0.0f};
	debugTongueRawNormal_ = {0.0f, 0.0f, 0.0f};
	debugTongueUsedNormal_ = {0.0f, 0.0f, 0.0f};
	debugTongueRawFaceName_ = "None";
	debugTongueUsedFaceName_ = "None";
}

void Player::RecordTongueHitDebug(int step, const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& rawNormal, const Vector3& usedNormal) {
	hasTongueHitDebug_ = true;
	debugTongueHitStep_ = step;
	debugTongueHitPoint_ = hitPoint;
	debugTongueRawNormal_ = Normalize3(rawNormal);
	debugTongueUsedNormal_ = Normalize3(usedNormal);
	debugTongueRawFaceName_ = DebugFaceNameFromNormal(block, debugTongueRawNormal_);
	debugTongueUsedFaceName_ = DebugFaceNameFromNormal(block, debugTongueUsedNormal_);
}

void Player::RefreshClingAnchorFromCurrentSurface(){
	if(!hasClingSurface_ || !tongue_){
		return;
	}

	Vector3 clingAnchorPoint =
		clingSurfaceCenter_
		+ clingSurfaceRight_ * clingHitRightOffset_
		+ clingSurfaceUp_ * clingHitUpOffset_;

	Vector3 hookPos = clingAnchorPoint + clingSurfaceNormal_ * tongueHookSurfaceOffset_;

	// 状態を Hooked に巻き戻さない
	tongue_->SetHookPositionPreserveState(hookPos);

	CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());
	const float kPullSkin = 0.03f;
	float playerRadiusAlongNormal =
		ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_);

	tonguePullTarget_ =
		clingAnchorPoint
		+ clingSurfaceNormal_ * (playerRadiusAlongNormal + kPullSkin);
}

void Player::ClearClingStageObjectTracking() {
	clingStageObjectId_ = -1;
	clingStageObjectIsMovingPlatform_ = false;
}

bool Player::IsPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin) const{
	if(!movementLimitCylinder_){
		return true;
	}

	// プレイヤーのXZ半径を保守的に見積もる
	float playerRadiusXZ = std::max(colliderHalfSize_.x, colliderHalfSize_.z) + extraMargin;
	float usableRadius = movementLimitCylinder_->radius - playerRadiusXZ;

	if(usableRadius <= 0.0f){
		return false;
	}

	float dx = position.x - movementLimitCylinder_->center.x;
	float dz = position.z - movementLimitCylinder_->center.z;
	float distSq = dx * dx + dz * dz;

	if(distSq > usableRadius * usableRadius){
		return false;
	}

	float minY = movementLimitCylinder_->center.y - movementLimitCylinder_->halfHeight + colliderHalfSize_.y;
	float maxY = movementLimitCylinder_->center.y + movementLimitCylinder_->halfHeight - colliderHalfSize_.y;

	return position.y >= minY && position.y <= maxY;
}

Vector3 Player::ClampPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin) const{
	if(!movementLimitCylinder_){
		return position;
	}

	Vector3 clamped = position;

	float playerRadiusXZ = std::max(colliderHalfSize_.x, colliderHalfSize_.z) + extraMargin;
	float usableRadius = movementLimitCylinder_->radius - playerRadiusXZ;

	if(usableRadius > 0.0f){
		float dx = clamped.x - movementLimitCylinder_->center.x;
		float dz = clamped.z - movementLimitCylinder_->center.z;
		float lenSq = dx * dx + dz * dz;

		if(lenSq > usableRadius * usableRadius && lenSq > 1e-8f){
			float len = std::sqrt(lenSq);
			float scale = usableRadius / len;
			clamped.x = movementLimitCylinder_->center.x + dx * scale;
			clamped.z = movementLimitCylinder_->center.z + dz * scale;
		}
	} else{
		clamped.x = movementLimitCylinder_->center.x;
		clamped.z = movementLimitCylinder_->center.z;
	}

	float minY = movementLimitCylinder_->center.y - movementLimitCylinder_->halfHeight + colliderHalfSize_.y;
	float maxY = movementLimitCylinder_->center.y + movementLimitCylinder_->halfHeight - colliderHalfSize_.y;
	clamped.y = ClampFloat(clamped.y, minY, maxY);

	return clamped;
}

void Player::UpdateJumpGaugeSprite(){
	if(!camera_ || !jumpGaugeBackSprite_ || !jumpGaugeFillSprite_){
		return;
	}

	showJumpGauge_ = isChargingJump_;
	if(!showJumpGauge_){
		return;
	}

	Vector3 worldPos = GetPosition();
	worldPos.y += 2.0f;

	Vector2 screenPos{};
	if(!WorldToScreenSimple(
		worldPos,
		camera_->GetViewProjectionMatrix(),
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
		screenPos)){
		showJumpGauge_ = false;
		return;
	}

	Vector2 gaugePos = {
		screenPos.x + jumpGaugeOffset_.x,
		screenPos.y + jumpGaugeOffset_.y
	};

	gaugePos.x = ClampFloat(
		gaugePos.x,
		static_cast<float>(WinApp::kClientWidth) * 0.4f,
		static_cast<float>(WinApp::kClientWidth) - jumpGaugeSize_.x - static_cast<float>(WinApp::kClientWidth) * 0.4f);

	gaugePos.y = ClampFloat(
		gaugePos.y,
		static_cast<float>(WinApp::kClientHeight) * 0.4f,
		static_cast<float>(WinApp::kClientHeight) - jumpGaugeSize_.y - static_cast<float>(WinApp::kClientHeight) * 0.4f);

	jumpGaugeBackSprite_->SetPos(gaugePos);
	jumpGaugeBackSprite_->SetSize(jumpGaugeSize_);
	jumpGaugeBackSprite_->Update();

	int allowedLevel = std::max(1, GetAllowedChargeLevel());
	int visibleLevel = GetCurrentVisibleChargeLevel();
	float phaseRate = ClampFloat(GetCurrentChargePhaseRate(), 0.0f, 1.0f);

	// 現在の段階色
	Vector4 segmentColors[4] = {
		{ 0.25f, 1.0f, 0.2f, 0.9f }, // 1段目
		{ 0.95f, 0.95f, 0.2f, 0.9f }, // 2段目
		{ 1.0f, 0.6f, 0.2f, 0.9f },   // 3段目
		{ 1.0f, 0.2f, 0.2f, 0.9f }    // 保険
	};

	int currentSegment = std::min(visibleLevel, allowedLevel - 1);

	jumpGaugeFillSprite_->SetPos(gaugePos);
	jumpGaugeFillSprite_->SetSize({
		jumpGaugeSize_.x * phaseRate,
		jumpGaugeSize_.y
								  });
	jumpGaugeFillSprite_->SetColor(segmentColors[std::min(currentSegment, 3)]);
	jumpGaugeFillSprite_->Update();
}

void Player::DrawUI(){
	if(!showJumpGauge_){
		return;
	}

	if(jumpGaugeBackSprite_){
		jumpGaugeBackSprite_->Draw();
	}

	if(jumpGaugeFillSprite_){
		jumpGaugeFillSprite_->Draw();
	}
}

void Player::ApplyClingSurfaceRotation() {
	if (!object_ || !hasClingSurface_) {
		return;
	}

	// 壁を「地面」として扱うので、
	// ローカルUpを面法線へ合わせる
	Vector3 desiredUp = Normalize3(clingSurfaceNormal_);

	// 面内の前方向は、既存の clingSurfaceUp_ を使う
	// ただし desiredUp と直交するように整え直す
	Vector3 desiredForward = clingSurfaceUp_
		- desiredUp * Dot3(clingSurfaceUp_, desiredUp);

	if (Length3(desiredForward) <= 0.0001f) {
		desiredForward = clingSurfaceUp_;
	}
	desiredForward = Normalize3(desiredForward);

	// 右方向を作る
	Vector3 desiredRight = Cross3(desiredForward, desiredUp);
	if (Length3(desiredRight) <= 0.0001f) {
		desiredRight = clingSurfaceRight_;
	}
	desiredRight = Normalize3(desiredRight);

	// 直交性をもう一度そろえる
	desiredForward = Normalize3(Cross3(desiredUp, desiredRight));

	desiredForward = desiredForward * 1.0f;
	desiredUp = desiredUp * 1.0f;
	desiredRight = desiredRight * -1.0f;

	Vector3 rot = ExtractEulerXYZFromBasis(
		desiredRight,
		desiredUp,
		desiredForward
	);

	object_->SetRotate(rot);
}

void Player::ApplyClingSurfaceRotationFacing(const Vector3& desiredForwardWorld) {
	if (!object_ || !hasClingSurface_) {
		return;
	}

	// 張り付き面を「地面」として扱う
	Vector3 desiredUp = Normalize3(clingSurfaceNormal_);

	// カメラ前方を面へ射影して、面内の前方向を作る
	Vector3 forwardProjected =
		desiredForwardWorld - desiredUp * Dot3(desiredForwardWorld, desiredUp);

	if (Length3(forwardProjected) <= 0.0001f) {
		forwardProjected = clingSurfaceUp_;
	}
	Vector3 desiredForward = Normalize3(forwardProjected);

	// 右方向を作る
	Vector3 desiredRight = Cross3(desiredForward, desiredUp);
	if (Length3(desiredRight) <= 0.0001f) {
		desiredRight = clingSurfaceRight_;
	}
	desiredRight = Normalize3(desiredRight);

	// 直交性を整え直す
	desiredForward = Normalize3(Cross3(desiredUp, desiredRight));

	Vector3 rot = ExtractEulerXYZFromBasis(
		-desiredRight,
		desiredUp,
		desiredForward
	);

	object_->SetRotate(rot);
}

void Player::UpdateClingStageObjectFromHitPoint(const Vector3& hitPoint) {
	ClearClingStageObjectTracking();

	if (!stage_) {
		return;
	}

	CollisionUtility::Sphere probe;
	probe.center = hitPoint;
	probe.radius = 0.08f;

	for (const auto& o : stage_->GetStageData().objects) {
		if (o.blockId != BlockID::MovingPlatform) {
			continue;
		}

		Transform t;
		t.translate = o.position;
		t.rotate = o.rotation;
		t.scale = o.scale;

		CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t, {1.0f, 1.0f, 1.0f});

		auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(probe, obb);
		if (!hit.hit) {
			continue;
		}

		clingStageObjectId_ = o.id;
		clingStageObjectIsMovingPlatform_ = true;
		return;
	}
}

void Player::RefreshMovingClingSurfaceFromStage() {
	if (!hasClingSurface_ || !clingStageObjectIsMovingPlatform_ || clingStageObjectId_ < 0 || !stage_) {
		return;
	}

	for (const auto& o : stage_->GetStageData().objects) {
		if (o.id != clingStageObjectId_) {
			continue;
		}

		if (o.blockId != BlockID::MovingPlatform) {
			ClearClingStageObjectTracking();
			return;
		}

		Transform t;
		t.translate = o.position;
		t.rotate = o.rotation;
		t.scale = o.scale;

		CollisionUtility::OBB newObb = CollisionUtility::MakeOBBFromTransform(t, {1.0f, 1.0f, 1.0f});

		Vector3 delta = newObb.center - clingBlockObb_.center;

		clingBlockObb_ = newObb;
		clingSurfaceCenter_ += delta;

		// すでに張り付いている最中はプレイヤー本体も一緒に運ぶ
		if (object_ && (moveState_ == MovementState::WallClinging || moveState_ == MovementState::CeilingCrawling)) {
			Vector3 pos = object_->GetTranslate();
			pos += delta;
			object_->SetTranslate(pos);
		}

		// 引っ張り中は目標点だけでも更新しておく
		RefreshClingAnchorFromCurrentSurface();
		return;
	}

	// 対象が消えていたら追跡解除
	ClearClingStageObjectTracking();
}