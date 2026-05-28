#include "Player.h"

#include "2d/SpriteCommon.h"
#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include "CameraController.h"
#include "Enemy/Core/BaseEnemy.h"
#include "Enemy/Manager/EnemyManager.h"
#include "Stage.h"
#include "debug/DebugRenderer.h"
#include "math/Vector4.h"
#include "utility/Logger.h"
#include <../externals/nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <limits>
#include <string>
#include <unordered_map>
#include "2d/TextureManager.h"
#include "UI/RuntimeTextTextureGenerator.h"
#include "base/WinApp.h"

Player::~Player() = default;

namespace {
float LengthXZ(const Vector3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }

float Length3(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3 Normalize3(const Vector3& v)
{
    float len = Length3(v);
    if (len <= 0.0001f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    return { v.x / len, v.y / len, v.z / len };
}

static Vector3 RotateY(const Vector3& v, float angleRad)
{
    float s = std::sin(angleRad);
    float c = std::cos(angleRad);
    return { v.x * c - v.z * s, v.y, v.x * s + v.z * c };
}

static float PointToSegmentDistSq(const Vector3& p, const Vector3& a, const Vector3& b)
{
    Vector3 ab = b - a;
    Vector3 ap = p - a;
    float abLen2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (abLen2 <= 1e-6f) {
        Vector3 d = p - a;
        return d.x * d.x + d.y * d.y + d.z * d.z;
    }
    float t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / abLen2;
    t = std::max(0.0f, std::min(1.0f, t));
    Vector3 proj = { a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t };
    Vector3 d = p - proj;
    return d.x * d.x + d.y * d.y + d.z * d.z;
}

bool WorldToScreenSimple(
    const Vector3& worldPos,
    const Matrix4x4& viewProjection,
    float screenW,
    float screenH,
    Vector2& outScreen)
{
    Vector4 clip {};
    clip.x = worldPos.x * viewProjection.m[0][0] + worldPos.y * viewProjection.m[1][0] + worldPos.z * viewProjection.m[2][0] + 1.0f * viewProjection.m[3][0];
    clip.y = worldPos.x * viewProjection.m[0][1] + worldPos.y * viewProjection.m[1][1] + worldPos.z * viewProjection.m[2][1] + 1.0f * viewProjection.m[3][1];
    clip.z = worldPos.x * viewProjection.m[0][2] + worldPos.y * viewProjection.m[1][2] + worldPos.z * viewProjection.m[2][2] + 1.0f * viewProjection.m[3][2];
    clip.w = worldPos.x * viewProjection.m[0][3] + worldPos.y * viewProjection.m[1][3] + worldPos.z * viewProjection.m[2][3] + 1.0f * viewProjection.m[3][3];

    if (clip.w <= 0.0001f) {
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
float Player::XPToNextLevel(int level) const
{
    const int lvInt = (std::max)(1, level);
    const float lv = static_cast<float>(lvInt);

    if (xpMode_ == XPMode::Odd) {
        // 1,3,5,7,... (odd numbers)
        // Lv1->2:1, Lv2->3:3, ...
        return xpOddScale_ * static_cast<float>(2 * lvInt - 1);
    }

    // default: base * level^growth (tunable via ability_config.json)
    return xpBase_ * std::pow(lv, xpGrowth_);
}

// 指定した能力に経験値を追加し、必要に応じてレベルアップを処理する関数
void Player::AddAbilityXP(AbilityId ability, float amount)
{
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
        pendingLevelUps_.push_back({ ability, s.level });
    }
}

void Player::EnqueueAbilityXP(AbilityId ability, float amount)
{
    if (amount <= 0.0f)
        return;
    pendingAbilityXP_.push_back({ ability, amount });
}

void Player::ApplyPendingAbilityXP()
{
    if (pendingAbilityXP_.empty())
        return;

    for (auto& p : pendingAbilityXP_) {
        AddAbilityXP(p.first, p.second);
    }
    pendingAbilityXP_.clear();
}

void Player::ApplyPendingLevelUps()
{
    if (pendingLevelUps_.empty())
        return;

    // Ability performance caps (these correspond to the original/standard baseline values)
    constexpr float kMaxTongueDistance = 30.0f;
    constexpr float kMaxSonarDuration = 3.0f;
    constexpr float kMaxWallClingGauge = 100.0f;
    constexpr float kMaxCamouflageDuration = 180.0f; // frames
    constexpr float kMaxJumpPowers[4] = { 0.55f, 0.70f, 0.80f, 1.10f };

    for (auto& lv : pendingLevelUps_) {
        AbilityId ability = lv.first;
        int newLevel = lv.second;

        // レベルアップに伴う能力の強化を反映する
        if (ability == AbilityId::JumpPower) {
            jumpPowerMultiplier_ = 1.0f + jumpPowerPerLevel_ * static_cast<float>(newLevel - 1);

            // Cap jump power so it never exceeds the original baseline values
            float capMul = std::numeric_limits<float>::infinity();
            for (int i = 0; i < 4; ++i) {
                if (baseJumpPowers_[i] > 1.0e-6f) {
                    capMul = (std::min)(capMul, kMaxJumpPowers[i] / baseJumpPowers_[i]);
                }
            }
            if (capMul != std::numeric_limits<float>::infinity()) {
                jumpPowerMultiplier_ = (std::min)(jumpPowerMultiplier_, capMul);
            }
        } else if (ability == AbilityId::TongueRange) {
            tongueRangeMultiplier_ = 1.0f + tongueRangePerLevel_ * static_cast<float>(newLevel - 1);
            if (tongue_) {
                float newMax = baseTongueMaxDistance_ * tongueRangeMultiplier_;
                newMax = (std::min)(newMax, kMaxTongueDistance);
                if (baseTongueMaxDistance_ > 1.0e-6f) {
                    tongueRangeMultiplier_ = newMax / baseTongueMaxDistance_;
                }
                tongue_->SetMaxDistance(newMax);
            }
        } else if (ability == AbilityId::SonarDuration) {
            sonarDurationMultiplier_ = 1.0f + sonarDurationPerLevel_ * static_cast<float>(newLevel - 1);

            float newDur = baseSonarDuration_ * sonarDurationMultiplier_;
            newDur = (std::min)(newDur, kMaxSonarDuration);
            if (baseSonarDuration_ > 1.0e-6f) {
                sonarDurationMultiplier_ = newDur / baseSonarDuration_;
            }
            sonarDuration_ = newDur;
        } else if (ability == AbilityId::WallClingDuration) {
            wallClingDurationMultiplier_ = 1.0f + wallClingPerLevel_ * static_cast<float>(newLevel - 1);

            float newGauge = baseWallClingGauge_ * wallClingDurationMultiplier_;
            newGauge = (std::min)(newGauge, kMaxWallClingGauge);
            if (baseWallClingGauge_ > 1.0e-6f) {
                wallClingDurationMultiplier_ = newGauge / baseWallClingGauge_;
            }
            maxWallClingGauge_ = newGauge;
            if (wallClingGauge_ < maxWallClingGauge_)
                wallClingGauge_ = maxWallClingGauge_;
        } else if (ability == AbilityId::CamouflageDuration) {
            camouflageDurationMultiplier_ = 1.0f + camouflagePerLevel_ * static_cast<float>(newLevel - 1);

            float newDur = baseCamouflageDuration_ * camouflageDurationMultiplier_;
            newDur = (std::min)(newDur, kMaxCamouflageDuration);
            if (baseCamouflageDuration_ > 1.0e-6f) {
                camouflageDurationMultiplier_ = newDur / baseCamouflageDuration_;
            }
        } else {
            // 不明な能力のレベルアップは無視する（安全策）
        }
    }

    pendingLevelUps_.clear();
}

void Player::LoadAbilityConfig()
{
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
                    auto& o = j["JumpPower"];
                    if (o.contains("perLevel"))
                        jumpPowerPerLevel_ = o["perLevel"].get<float>();
                }
                if (j.contains("XP")) {
                    auto& o = j["XP"];
                    if (o.contains("base"))
                        xpBase_ = o["base"].get<float>();
                    if (o.contains("growth"))
                        xpGrowth_ = o["growth"].get<float>();
                   if (o.contains("mode")) {
                        std::string mode = o["mode"].get<std::string>();
                        if (mode == "Odd" || mode == "odd") {
                            xpMode_ = XPMode::Odd;
                        } else {
                            xpMode_ = XPMode::Power;
                        }
                    }
                    if (o.contains("oddScale")) {
                        xpOddScale_ = o["oddScale"].get<float>();
                        if (xpOddScale_ < 0.0f)
                            xpOddScale_ = 0.0f;
                    }
                }
                if (j.contains("TongueRange")) {
                    auto& o = j["TongueRange"];
                    if (o.contains("base"))
                        baseTongueMaxDistance_ = o["base"].get<float>();
                    if (o.contains("perLevel"))
                        tongueRangePerLevel_ = o["perLevel"].get<float>();
                }
                if (j.contains("SonarDuration")) {
                    auto& o = j["SonarDuration"];
                    if (o.contains("base"))
                        baseSonarDuration_ = o["base"].get<float>();
                    if (o.contains("perLevel"))
                        sonarDurationPerLevel_ = o["perLevel"].get<float>();
                }
                if (j.contains("WallClingDuration")) {
                    auto& o = j["WallClingDuration"];
                    if (o.contains("base"))
                        baseWallClingGauge_ = o["base"].get<float>();
                    if (o.contains("perLevel"))
                        wallClingPerLevel_ = o["perLevel"].get<float>();
                }
                if (j.contains("CamouflageDuration")) {
                    auto& o = j["CamouflageDuration"];
                    if (o.contains("base"))
                        baseCamouflageDuration_ = o["base"].get<float>();
                    if (o.contains("perLevel"))
                        camouflagePerLevel_ = o["perLevel"].get<float>();
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

bool Player::TryUseBeam(const Vector3& direction)
{
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

    std::vector<std::pair<BaseEnemy*, int>> hitList;

    for (int i = 0; i < samples; ++i) {
        float t = (samples == 1) ? 0.0f : (static_cast<float>(i) / static_cast<float>(samples - 1));
        float angle = -halfAngleRad + t * (2.0f * halfAngleRad);
        Vector3 dir = Normalize3(direction);
        Vector3 sweepDir = RotateY(dir, angle);
        Vector3 segEnd = origin + sweepDir * beamRange_;

        // (removed immediate debug draw here to avoid duplicate visuals; active sweep will draw)

        // Check against enemies
        enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
            if (!e)
                return;

            for (const auto& part : e->GetTargetParts(0.7f)) {
                // avoid double-hitting
                bool alreadyHit = false;
                for (auto h : hitList) {
                    if (h.first == e && h.second == part.partId) {
                        alreadyHit = true;
                        break;
                    }
                }
                if (alreadyHit)
                    continue;

                Vector3 ep = part.position;
                float distSq = PointToSegmentDistSq(ep, origin, segEnd);
                const float enemyRadius = 0.7f; // approximate
                float hitRadius = beamCapsuleRadius_ + enemyRadius;
                if (distSq <= hitRadius * hitRadius) {
                    hitList.push_back({e, part.partId});
                }
            }
        });
    }

    // Apply effect immediately for instant hit (keep behavior) THEN start animated sweep for visuals
    for (auto h : hitList) {
        if (h.first)
            h.first->KillPart(h.second);
    }

    // Start animated beam sweep in parallel for visuals and extended hit processing
    if (!StartBeamActive(direction)) {
        // if cannot start animated sweep (e.g., resources), still return success of instant hit
    }

    return true;
}

bool Player::StartBeamActive(const Vector3& beamDir)
{
    if (!enemyManager_) {
        return false;
    }

    // Initialize animated sweep state
    // If tongue is sweeping, sync beam duration to tongue sweep duration (frames)
    if (tongue_ && tongue_->IsSweeping()) {
        float dur = tongue_->GetSweepDuration();
        if (dur > 0.001f) {
            // sync beam duration to tongue sweep but do not make it faster than base
            float candidate = dur * 60.0f; // seconds->frames
            beamActiveDuration_ = std::max(beamBaseActiveDuration_, candidate);
        }
    }
    beamActiveTimer_ = beamActiveDuration_;
    beamActiveOrigin_ = GetPosition();
    beamActiveDir_ = beamDir;
    beamActiveHalfAngleDeg_ = beamHalfAngleDeg_;
    // ensure active radius can cover tongue reach if tongue sweep is used
    beamActiveMaxRadius_ = beamRange_;
    if (tongue_ && tongue_->IsSweeping()) {
        // allow beam active max to at least cover tongue max distance + its extra extend
        float tongueMaxReach = tongue_->GetMaxDistance() + tongue_->GetExtraExtendDistance();
        if (tongueMaxReach > beamActiveMaxRadius_)
            beamActiveMaxRadius_ = tongueMaxReach;
    }
    beamActiveHitList_.clear();

    // store original capsule radius
    beamOriginalCapsuleRadius_ = beamCapsuleRadius_;

    return true;
}

float Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float ComputeOBBSupportRadiusAlongNormal(const CollisionUtility::OBB& obb, const Vector3& normal)
{
    return std::abs(Dot3(obb.axis[0], normal)) * obb.halfLength[0] + std::abs(Dot3(obb.axis[1], normal)) * obb.halfLength[1] + std::abs(Dot3(obb.axis[2], normal)) * obb.halfLength[2];
}

float AbsDot3(const Vector3& a, const Vector3& b) { return std::abs(Dot3(a, b)); }

Vector3 Cross3(const Vector3& a, const Vector3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

float ClampFloat(float v, float minV, float maxV) { return std::max(minV, std::min(v, maxV)); }

bool IsPointInsideShrunkOBBForTongue(
    const Vector3& point,
    const CollisionUtility::OBB& block,
    float shrink
)
{
    Vector3 axis[3] = {
        Normalize3(block.axis[0]),
        Normalize3(block.axis[1]),
        Normalize3(block.axis[2]),
    };

    Vector3 localVec = point - block.center;

    for (int i = 0; i < 3; ++i) {
        float half = std::max(0.0f, block.halfLength[i] - shrink);
        float local = Dot3(localVec, axis[i]);

        if (std::abs(local) > half) {
            return false;
        }
    }

    return true;
}

bool DoesSegmentEnterShrunkOBBForTongue(
    const Vector3& start,
    const Vector3& end,
    const CollisionUtility::OBB& block,
    float shrink
)
{
    if (IsPointInsideShrunkOBBForTongue(start, block, shrink) ||
        IsPointInsideShrunkOBBForTongue(end, block, shrink)) {
        return true;
    }

    Vector3 axis[3] = {
        Normalize3(block.axis[0]),
        Normalize3(block.axis[1]),
        Normalize3(block.axis[2]),
    };

    Vector3 localStartVec = start - block.center;
    Vector3 segment = end - start;

    float tMin = 0.0f;
    float tMax = 1.0f;

    for (int i = 0; i < 3; ++i) {
        float half = std::max(0.0f, block.halfLength[i] - shrink);
        float origin = Dot3(localStartVec, axis[i]);
        float dir = Dot3(segment, axis[i]);

        float minV = -half;
        float maxV = half;

        if (std::abs(dir) <= 0.000001f) {
            if (origin < minV || origin > maxV) {
                return false;
            }
            continue;
        }

        float t1 = (minV - origin) / dir;
        float t2 = (maxV - origin) / dir;

        if (t1 > t2) {
            std::swap(t1, t2);
        }

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax) {
            return false;
        }
    }

    return tMax >= 0.0f && tMin <= 1.0f;
}

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

    velocity_ = { 0.0f, 0.0f, 0.0f };
    lastMove_ = { 0.0f, 0.0f, 0.0f };
    isOnGround_ = false;

    waterGauge_ = maxWaterGauge_;
    hp_ = maxHp_;
    enemyContactInvincibilityTimer_ = 0.0f;

    isChargingJump_ = false;
    isJumpChargeCanceled_ = false;
    chargeTimer_ = 0.0f;
    chargeMaxHoldTimer_ = 0.0f;
    jumpChargeAirCancelGraceTimer_ = 0;
    moveState_ = MovementState::Root;
    wallClingGauge_ = maxWallClingGauge_;
    prevAimMode_ = false;
    InitializeUI(SpriteCommon::GetInstance(), "white");
}

void Player::InitializeUI(SpriteCommon* spriteCommon, const std::string& gaugeTextureFilePath)
{
    if (!spriteCommon) {
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

    hpGaugeBackSprite_ = std::make_unique<Sprite>();
    hpGaugeBackSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
    hpGaugeBackSprite_->SetPos(hpGaugePos_);
    hpGaugeBackSprite_->SetSize(hpGaugeSize_);
    hpGaugeBackSprite_->SetColor({ 0.05f, 0.05f, 0.05f, 0.75f });

    hpGaugeFillSprite_ = std::make_unique<Sprite>();
    hpGaugeFillSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
    hpGaugeFillSprite_->SetPos(hpGaugePos_);
    hpGaugeFillSprite_->SetSize(hpGaugeSize_);
    hpGaugeFillSprite_->SetColor({ 0.85f, 0.15f, 0.15f, 0.95f });

    hpGaugeFrameSprite_ = std::make_unique<Sprite>();
    hpGaugeFrameSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
    hpGaugeFrameSprite_->SetPos({
        hpGaugePos_.x - hpGaugeFrameThickness_,
        hpGaugePos_.y - hpGaugeFrameThickness_
        });
    hpGaugeFrameSprite_->SetSize({
        hpGaugeSize_.x + hpGaugeFrameThickness_ * 2.0f,
        hpGaugeSize_.y + hpGaugeFrameThickness_ * 2.0f
        });
    hpGaugeFrameSprite_->SetColor({ 0.95f, 0.95f, 0.95f, 0.9f });

    wallClingGaugeBackSprite_ = std::make_unique<Sprite>();
    wallClingGaugeBackSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
    wallClingGaugeBackSprite_->SetPos(wallClingGaugePos_);
    wallClingGaugeBackSprite_->SetSize(wallClingGaugeSize_);
    wallClingGaugeBackSprite_->SetColor({ 0.10f, 0.10f, 0.10f, 0.75f });

    wallClingGaugeFillSprite_ = std::make_unique<Sprite>();
    wallClingGaugeFillSprite_->Initialize(spriteCommon, gaugeTextureFilePath);
    wallClingGaugeFillSprite_->SetPos(wallClingGaugePos_);
    wallClingGaugeFillSprite_->SetSize(wallClingGaugeSize_);
    wallClingGaugeFillSprite_->SetColor({ 0.95f, 0.85f, 0.15f, 0.95f });

    InitializeAbilityLevelUI(spriteCommon);
}

const char* Player::GetAbilityLevelIconTexturePath(AbilityId ability) const
{
    switch (ability) {
    case AbilityId::JumpPower:
        return "resources/UI/JumpingPower.png";
    case AbilityId::SonarDuration:
        return "resources/UI/SonarDuration.png";
    case AbilityId::WallClingDuration:
        return "resources/UI/Sticking.png";
    case AbilityId::TongueRange:
        return "resources/UI/BottomLength.png";
    case AbilityId::CamouflageDuration:
        return "resources/UI/Mimicry.png";
    default:
        return "white";
    }
}

void Player::InitializeAbilityLevelUI(SpriteCommon* spriteCommon)
{
    if (!spriteCommon) {
        return;
    }

    // "lv." の小さなテクスチャを1回だけ生成
    if (!std::filesystem::exists(abilityLevelUILvPrefixTexturePath_)) {
        RuntimeTextTextureGenerator::GenerateDesc desc;
        desc.textUtf8 = "lv.";
        desc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
        desc.outputFilePath = abilityLevelUILvPrefixTexturePath_;
        desc.fontPixelSize = 22;
        desc.paddingX = 4;
        desc.paddingY = 2;
        desc.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        desc.shadowColor = { 0.0f, 0.0f, 0.0f, 0.6f };
        desc.shadowOffsetX = 2;
        desc.shadowOffsetY = 2;
        RuntimeTextTextureGenerator::GeneratePng(desc);
    }

    TextureManager::GetInstance()->LoadTexture(abilityLevelUILvPrefixTexturePath_);
    TextureManager::GetInstance()->LoadTexture("resources/UI/KiwiMaruNumStrength.png");

    abilityLevelUIEntries_.clear();
    abilityLevelUIEntries_.reserve(abilityLevelUIOrder_.size());

    for (AbilityId ability : abilityLevelUIOrder_) {
        const char* iconPath = GetAbilityLevelIconTexturePath(ability);
        TextureManager::GetInstance()->LoadTexture(iconPath);

        AbilityLevelUIEntry entry;
        entry.ability = ability;

        entry.iconSprite = std::make_unique<Sprite>();
        entry.iconSprite->Initialize(spriteCommon, iconPath);
        entry.iconSprite->SetAnchorPoint({ 0.0f, 0.0f });
        entry.iconSprite->SetSize(abilityLevelUIIconSize_);

        entry.xpFillSprite = std::make_unique<Sprite>();
        entry.xpFillSprite->Initialize(spriteCommon, "white");
        entry.xpFillSprite->SetAnchorPoint({ 0.0f, 0.0f });
        entry.xpFillSprite->SetColor(abilityLevelUIXPFillColor_);

        entry.lvPrefixSprite = std::make_unique<Sprite>();
        entry.lvPrefixSprite->Initialize(spriteCommon, abilityLevelUILvPrefixTexturePath_);
        entry.lvPrefixSprite->SetAnchorPoint({ 0.0f, 0.0f });
        entry.lvPrefixSprite->SetSize(abilityLevelUILvPrefixSize_);

        entry.levelNumberText.Initialize(
            spriteCommon,
            "resources/UI/KiwiMaruNumStrength.png",
            2
        );
        entry.levelNumberText.SetDigitSize(abilityLevelUINumberSize_);
        entry.levelNumberText.SetSpacing(0.0f);
        entry.levelNumberText.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        entry.stateOverlaySprite = std::make_unique<Sprite>();
        entry.stateOverlaySprite->Initialize(spriteCommon, "white");
        entry.stateOverlaySprite->SetAnchorPoint({ 0.0f, 0.0f });
        entry.stateOverlaySprite->SetSize(abilityLevelUIIconSize_);
        entry.stateOverlaySprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

        abilityLevelUIEntries_.push_back(std::move(entry));
    }
}

void Player::ResolveMovementLimitCylinder()
{
    if (!object_ || !movementLimitCylinder_) {
        return;
    }

    Vector3 position = object_->GetTranslate();

    if (CollisionUtility::IsPointInsideCylinder(position, *movementLimitCylinder_)) {
        return;
    }

    position = CollisionUtility::ClosestPointInsideCylinder(position, *movementLimitCylinder_);

    // 念のため少しだけ内側へ寄せる
    Vector3 toCenter = { movementLimitCylinder_->center.x - position.x, 0.0f, movementLimitCylinder_->center.z - position.z };

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

bool Player::CanStartTongueShot() const
{
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

Vector3 Player::ResolveHookSurfaceNormal(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& tongueDelta, const Vector3& playerPos) const
{
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
        { axis[0], std::abs(block.halfLength[0] - local[0]) },
        { axis[0] * -1, std::abs(-block.halfLength[0] - local[0]) },
        { axis[1], std::abs(block.halfLength[1] - local[1]) },
        { axis[1] * -1, std::abs(-block.halfLength[1] - local[1]) },
        { axis[2], std::abs(block.halfLength[2] - local[2]) },
        { axis[2] * -1, std::abs(-block.halfLength[2] - local[2]) },
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

void Player::ResolveHookSurfaceFromPlayerCapsule(
    const CollisionUtility::OBB& block,
    const Vector3& playerPos,
    const Vector3& tongueHitPoint,
    const Vector3& tongueDelta,
    Vector3& outNormal,
    Vector3& outHitPoint
) const
{
    Vector3 axis[3] = {
        Normalize3(block.axis[0]),
        Normalize3(block.axis[1]),
        Normalize3(block.axis[2]),
    };

    CollisionUtility::OBB playerObb = GetPlayerOBB(playerPos);

    Vector3 travelDir = Normalize3(tongueDelta);
    Vector3 centerToPlayer = Normalize3(playerPos - block.center);

    // 舌先ではなく、プレイヤー本体側のカプセル相当点を使う
    // yだけは舌が当たった高さを参考にするが、プレイヤーの上下範囲内に制限する
// 面を選ぶための基準点。
// これはプレイヤー本体側を見るためのもの。
    Vector3 playerReferencePoint = playerPos;
    playerReferencePoint.y = ClampFloat(
        tongueHitPoint.y,
        playerPos.y - colliderHalfSize_.y,
        playerPos.y + colliderHalfSize_.y
    );

    Vector3 playerLocalVec = playerReferencePoint - block.center;
    float playerLocal[3] = {
        Dot3(playerLocalVec, axis[0]),
        Dot3(playerLocalVec, axis[1]),
        Dot3(playerLocalVec, axis[2]),
    };

    // 舌が実際に当たった位置。
    // 最終的なフック位置はこちらを基準にする。
    Vector3 tongueLocalVec = tongueHitPoint - block.center;
    float tongueLocal[3] = {
        Dot3(tongueLocalVec, axis[0]),
        Dot3(tongueLocalVec, axis[1]),
        Dot3(tongueLocalVec, axis[2]),
    };

    int bestAxisIndex = 0;
    float bestSign = 1.0f;
    Vector3 bestNormal = axis[0];
    float bestScore = -1.0e9f;

    for (int i = 0; i < 3; ++i) {
        for (int signIndex = 0; signIndex < 2; ++signIndex) {
            float sign = signIndex == 0 ? 1.0f : -1.0f;
            Vector3 n = axis[i] * sign;

            Vector3 faceCenter = block.center + n * block.halfLength[i];

            // この値が大きいほど、プレイヤー中心がその面の外側にいる
            float centerSide = Dot3(playerPos - faceCenter, n);

            // プレイヤーの当たり判定サイズぶんを考慮した距離
            float playerRadiusAlongNormal = ComputeOBBSupportRadiusAlongNormal(playerObb, n);
            float surfaceDistance = centerSide - playerRadiusAlongNormal;

            // 面の範囲外に大きく外れている候補は弱くする
            float tangentPenalty = 0.0f;
            for (int k = 0; k < 3; ++k) {
                if (k == i) {
                    continue;
                }

                float over = std::abs(tongueLocal[k]) - block.halfLength[k];
                if (over > 0.0f) {
                    tangentPenalty += over;
                }
            }

            // プレイヤー中心から見て自然な面を最優先
            float playerSideScore = Dot3(n, centerToPlayer);

            // 舌方向は補助。主役にしすぎない
            float tongueScore = Dot3(n, travelDir * -1.0f);

            float score = 0.0f;
            score += playerSideScore * 2.0f;
            score += centerSide * 0.45f;
            score += tongueScore * 0.20f;
            score -= tangentPenalty * 0.50f;
            score -= std::abs(surfaceDistance) * 0.05f;

            // プレイヤーから見て明らかにブロックの反対側にある面は強く落とす
            if (centerSide < -playerRadiusAlongNormal * 0.5f) {
                score -= 5.0f;
            }

            if (score > bestScore) {
                bestScore = score;
                bestAxisIndex = i;
                bestSign = sign;
                bestNormal = n;
            }
        }
    }

    outNormal = Normalize3(bestNormal);

    // 選ばれた面の表面上に、張り付き用の点を作る
    float outLocal[3] = {
        tongueLocal[0],
        tongueLocal[1],
        tongueLocal[2],
    };

    outLocal[bestAxisIndex] = bestSign * block.halfLength[bestAxisIndex];

    for (int i = 0; i < 3; ++i) {
        if (i == bestAxisIndex) {
            continue;
        }

        outLocal[i] = ClampFloat(
            outLocal[i],
            -block.halfLength[i],
            block.halfLength[i]
        );
    }

    outHitPoint =
        block.center
        + axis[0] * outLocal[0]
        + axis[1] * outLocal[1]
        + axis[2] * outLocal[2];
}
void Player::SetCamera(Camera* camera)
{
    camera_ = camera;
    if (object_) {
        object_->SetCamera(camera_);
    }
}

void Player::SetPosition(const Vector3& position)
{
    if (object_) {
        object_->SetTranslate(position);
    }
}

void Player::SetPendingTeleport(const Vector3& position)
{
    pendingTeleport_ = true;
    pendingTeleportPosition_ = position;
}

void Player::SetRidingPlatformDelta(const Vector3& delta) { ridingPlatformDelta_ = delta; }

void Player::ClearRidingPlatformDelta() { ridingPlatformDelta_ = { 0.0f, 0.0f, 0.0f }; }

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

Vector3 Player::GetRotate() const
{
    if (!object_) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return object_->GetRotate();
};

CollisionUtility::OBB Player::GetPlayerOBB(const Vector3& position) const
{
    Transform t;
    t.translate = position;
    t.rotate = object_ ? object_->GetRotate() : Vector3 { 0.0f, 0.0f, 0.0f };
    t.scale = { 1.0f, 1.0f, 1.0f };

    return CollisionUtility::MakeOBBFromTransform(t, colliderHalfSize_);
}

void Player::AddWater(float amount)
{
    waterGauge_ += amount;
    if (waterGauge_ > maxWaterGauge_) {
        waterGauge_ = maxWaterGauge_;
    }
    if (waterGauge_ < 0.0f) {
        waterGauge_ = 0.0f;
    }
}

bool Player::ConsumeWater(float amount)
{
    if (waterGauge_ < amount) {
        return false;
    }
    waterGauge_ -= amount;
    if (waterGauge_ < 0.0f) {
        waterGauge_ = 0.0f;
    }
    return true;
}

void Player::ApplyDamage(int damage)
{
    if (damage <= 0) {
        return;
    }

    if (enemyContactInvincibilityTimer_ > 0.0f) {
        return;
    }

    hp_ -= damage;
    if (hp_ < 0) {
        hp_ = 0;
    }

    enemyContactInvincibilityTimer_ = enemyContactInvincibilityFrames_;
}

bool Player::TryShotTongue(const Vector3& direction)
{
    if (!CanStartTongueShot()) {
        return false;
    }

    if (!ConsumeWater(tongueWaterCost_)) {
        return false;
    }

    tongue_->Shot(direction);
    return true;
}

void Player::SetYawFromCamera(float cameraYaw)
{
    if (!object_) {
        return;
    }

    object_->SetRotate({ 0.0f, cameraYaw + modelYawOffset_, 0.0f });
}

void Player::ResolveHorizontalCollisions(const Vector3& previousPosition)
{
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
        Vector3 testPos = { position.x, previousPosition.y + kGroundEpsilon, previousPosition.z };

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
        Vector3 testPos = { position.x, previousPosition.y + kGroundEpsilon, position.z };

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

void Player::ResolveVerticalCollisions(const Vector3& previousPosition)
{
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

void Player::CheckTongueBlockHook()
{
    if (!tongue_ || !blockColliders_) {
        return;
    }

    if (tongue_->GetState() != Tongue::State::Extending) {
        return;
    }

    if (tongue_->IsSweeping()) {
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
            int hitEnemyPartId = 0;
            Vector3 hookPos = { 0, 0, 0 };
            enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
                if (hitEnemy || !e)
                    return;
                // フック可能な敵（センチネル・フック等）かチェック
                if (!e->IsGrappable())
                    return;

                for (const auto& part : e->GetTargetParts(0.8f)) {
                    auto hitE = CollisionUtility::IntersectSphere_OBB_Detailed(testSphere, part.obb);
                    if (hitE.hit) {
                        hitEnemy = e;
                        hitEnemyPartId = part.partId;
                        hookPos = part.position;
                        break;
                    }
                }
            });

            if (hitEnemy) {
                // 敵に「刺さった方向」を伝える（これで敵が吹き飛ぶ！等）
                Vector3 shotDir = Normalize3(delta);
                hitEnemy->OnTongueHit(shotDir);

                lastHitEnemy_ = hitEnemy; // スリングショット用にこの敵を記憶
                lastHitEnemyPartId_ = hitEnemyPartId;
                tonguePullingEnemy_ = true;
                tongue_->SetHooked(hookPos);
                tonguePullTarget_ = hookPos; // 敵の位置を目標にセット

                velocity_ = { 0.0f, 0.0f, 0.0f };
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
            Vector3 usedHitNormal = rawHitNormal;
            Vector3 usedHitPoint = hit.point;

            const float kTonguePenetrationShrink = 0.02f;

            // 舌先中心がブロック本体に入った時だけ「貫通」とみなす。
            // Sphere が表面に触れただけなら、中心はまだ外側なので false になる。
            bool tonguePenetratedBlock =
                DoesSegmentEnterShrunkOBBForTongue(
                    start,
                    testSphere.center,
                    block,
                    kTonguePenetrationShrink
                );

            if (!debugIgnoreHookSurfaceCorrection_ && tonguePenetratedBlock) {
                ResolveHookSurfaceFromPlayerCapsule(
                    block,
                    GetPosition(),
                    hit.point,
                    delta,
                    usedHitNormal,
                    usedHitPoint
                );
            }

            if (IsGroundSurface(usedHitNormal)) {
                tongue_->StartReturn();
                return;
            }

            SetupClingSurfaceFromHit(block, usedHitPoint, usedHitNormal);
            UpdateClingStageObjectFromHitPoint(usedHitPoint);

            Vector3 hookPos = {};

            if (tonguePenetratedBlock) {
                // 貫通時だけ、補正後の面上アンカーを使う
                Vector3 hookAnchorPoint =
                    clingSurfaceCenter_
                    + clingSurfaceRight_ * clingHitRightOffset_
                    + clingSurfaceUp_ * clingHitUpOffset_;

                hookPos = hookAnchorPoint + clingSurfaceNormal_ * tongueHookSurfaceOffset_;
            }
            else {
                // 通常ヒット時は、元の舌ヒット位置をなるべくそのまま使う
                hookPos = usedHitPoint + usedHitNormal * tongueHookSurfaceOffset_;
            }

            tongue_->SetHooked(hookPos);

            // 擬態処理 (Camouflage)
            if (abilityActive_ && currentAbility_ == Ability::Camouflage && stage_) {
                CollisionUtility::Sphere stageProbe;
                stageProbe.center = usedHitPoint;
                stageProbe.radius = 0.05f;

                for (const auto& o : stage_->GetStageData().objects) {
                    if (o.modelName.empty())
                        continue;
                    Transform t_obj;
                    t_obj.translate = o.position;
                    t_obj.rotate = o.rotation;
                    t_obj.scale = o.scale;
                    CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t_obj, { 1.0f, 1.0f, 1.0f });
                    auto hitObj = CollisionUtility::IntersectSphere_OBB_Detailed(stageProbe, obb);
                    if (hitObj.hit) {
                        mimicModelName_ = o.modelName;
                        mimicColor_ = o.color;
                        mimicColor_.w = 1.0f;
                        mimicScale_ = { 1.0f, 1.0f, 1.0f };
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

            if (!IsPlayerPositionInsideMovementCylinder(tonguePullTarget_, -1.00f)) {
                tongue_->StartReturn();
                hasClingSurface_ = false;
                ClearClingStageObjectTracking();
                return;
            }

            velocity_ = { 0.0f, 0.0f, 0.0f };
            CancelJumpCharge();

			if (useTonguePull_) {
                tonguePullingEnemy_ = false;
				TransitionTo(MovementState::TonguePulling);
			}
			return;
		}
	}
}

void Player::CheckEnemyContactDamage()
{
    if (!enemyManager_ || !object_) {
        return;
    }

    if (enemyContactInvincibilityTimer_ > 0.0f) {
        return;
    }

    CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());

    bool hitEnemy = false;

    enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
        if (hitEnemy || !e) {
            return;
        }

        for (const auto& part : e->GetTargetParts(0.8f)) {
            if (CollisionUtility::IntersectOBB_OBB(playerObb, part.obb)) {
                hitEnemy = true;
                break;
            }
        }
        });

    if (hitEnemy) {
        ApplyDamage(enemyContactDamage_);
    }
}

bool Player::CheckTongueBlockDamage() {
	if (!tongue_ || !stage_ || !breakableBlockColliders_) {
		return false;
	}

	if (tongue_->GetState() != Tongue::State::Extending) {
		return false;
	}

	const CollisionUtility::Sphere tipSphere = tongue_->GetHitSphere();
	const float hitRadius = tipSphere.radius;

	// 通常ショットは舌先だけ
	if (!tongue_->IsSweeping()) {
		for (const auto& obb : *breakableBlockColliders_) {
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

    if (tonguePullingEnemy_ &&
        lastHitEnemy_ &&
        enemyManager_ &&
        !enemyManager_->ContainsAliveEnemy(lastHitEnemy_)) {

        lastHitEnemy_ = nullptr;
    }

    if (tonguePullingEnemy_ && !lastHitEnemy_) {
        tonguePullingEnemy_ = false;

        if (tongue_) {
            tongue_->StartReturn();
        }

        velocity_ = { 0.0f, 0.0f, 0.0f };
        TransitionTo(MovementState::Jumping);
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
            velocity_.y += baseJumpPowers_[0] * jumpPowerMultiplier_ * 2.0f; // 少し上に跳ね上げる (upgrade applied)

            lastHitEnemy_->KillPart(lastHitEnemyPartId_);

            // 重要なのは「遷移する前に情報をクリアする」こと
            tonguePullingEnemy_ = false;
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
    object_->SetRotate({ 0.0f, yaw, 0.0f });
}

void Player::Update()
{
    if (!object_) {
        return;
    }

    suppressTongueShotThisFrame_ = false;

    if (hp_ <= 0) {
		isDead_ = true;
    }

    if (!abilityConfigLoaded_) {
        LoadAbilityConfig();
    }

    // 経験値とレベルアップの適用
    ApplyPendingAbilityXP();
    ApplyPendingLevelUps();

    float cameraYaw = 0.0f;
    bool isAimMode = false;
    Vector3 cameraForward = { 0.0f, 0.0f, 1.0f };

    if (cameraController_) {
        cameraYaw = cameraController_->GetYaw();
        isAimMode = cameraController_->IsAimMode();
        cameraForward = cameraController_->GetForwardDirection();
    }

    if (pendingTeleport_) {
        // ワープ開始位置と目標位置を記録
        warpStartPosition_ = object_->GetTranslate();
        warpTargetPosition_ = pendingTeleportPosition_;
        warpTimer_ = 0.0f;

        velocity_ = { 0.0f, 0.0f, 0.0f };
        groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
        lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };

        isOnGround_ = false;

        // ワープ時は舌・張り付き状態を強制解除する
        if (tongue_) {
            tongue_->Reset();
        }

        hasClingSurface_ = false;
        lastHitEnemy_ = nullptr;
        ClearClingStageObjectTracking();
        CancelJumpCharge();

        TransitionTo(MovementState::Warping);
        pendingTeleport_ = false;
    }

    if (isMimicking_ && mimicTimer_ > 0.0f) {
        mimicTimer_ -= 1.0f; // Update is per-frame
        if (mimicTimer_ <= 0.0f) {
            EndMimic();
        }
    }

    //if (ridingPlatformDelta_.x != 0.0f || ridingPlatformDelta_.y != 0.0f || ridingPlatformDelta_.z != 0.0f) {
    //    Vector3 pos = object_->GetTranslate();
    //    pos.x += ridingPlatformDelta_.x;
    //    pos.y += ridingPlatformDelta_.y;
    //    pos.z += ridingPlatformDelta_.z;
    //    object_->SetTranslate(pos);

    //    const float kInvDt = 60.0f;
    //    velocity_.x += ridingPlatformDelta_.x * kInvDt;
    //    velocity_.z += ridingPlatformDelta_.z * kInvDt;
    //    if (std::fabs(ridingPlatformDelta_.y) > 1e-6f)
    //        velocity_.y = 0.0f;

    //    Logger::Log(
    //        std::string("Player applied riding delta early posAfter:") + std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z) + " delta:" + std::to_string(ridingPlatformDelta_.x) + "," + std::to_string(ridingPlatformDelta_.y) + "," + std::to_string(ridingPlatformDelta_.z) + "\n");

    //    ridingPlatformDelta_ = { 0.0f, 0.0f, 0.0f };
    //}

    // --- センチネル追従ロジック ---
    if (moveState_ == MovementState::TonguePulling) {

        if (tonguePullingEnemy_ &&
            lastHitEnemy_ &&
            enemyManager_ &&
            !enemyManager_->ContainsAliveEnemy(lastHitEnemy_)) {

            lastHitEnemy_ = nullptr;
        }

        if (tonguePullingEnemy_ && !lastHitEnemy_) {
            tonguePullingEnemy_ = false;
            velocity_ = { 0.0f, 0.0f, 0.0f };

            if (tongue_) {
                tongue_->StartReturn();
            }

            TransitionTo(MovementState::Jumping);
        }
        else {
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
                    velocity_ = lastHitEnemy_->GetVelocity();
                    velocity_.y += baseJumpPowers_[0] * jumpPowerMultiplier_;
                    lastHitEnemy_->KillPart(lastHitEnemyPartId_);
                    lastHitEnemy_ = nullptr;
                    tonguePullingEnemy_ = false;
                }

                if (tongue_) {
                    tongue_->StartReturn();
                }

                TransitionTo(MovementState::Jumping);
            }
        }
    }

    if (moveState_ == MovementState::WallClinging || moveState_ == MovementState::CeilingCrawling || moveState_ == MovementState::TonguePulling) {
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
            jumpChargeAirCancelGraceTimer_ = 0;
            TransitionTo(MovementState::Root);
        }
        else {
            // 地面から離れた瞬間。
            // 溜め中なら、即キャンセルせず3Fだけ猶予を持たせる。
            if (isChargingJump_) {
                jumpChargeAirCancelGraceTimer_ = jumpChargeAirCancelGraceFrames_;
            }
            else {
                jumpChargeAirCancelGraceTimer_ = 0;
            }

            TransitionTo(MovementState::Jumping);
        }
		break;
	}

    case MovementState::Jumping: {
        Vector3 previousPosition = object_->GetTranslate();

        UpdateAirborneHorizontalMove(cameraYaw);
        ResolveHorizontalCollisions(previousPosition);
        ResolveMovementLimitCylinder();

        Vector3 beforeVertical = object_->GetTranslate();
        ApplyGravity();
        ResolveVerticalCollisions(beforeVertical);
        ResolveMovementLimitCylinder();

        if (!isOnGround_ && isChargingJump_) {
            if (jumpChargeAirCancelGraceTimer_ > 0) {
                --jumpChargeAirCancelGraceTimer_;
            }
            else {
                // 空中に出てから猶予を超えたので溜めをキャンセル
                CancelJumpCharge();
            }
        }

        if (isOnGround_) {
            jumpChargeAirCancelGraceTimer_ = 0;
        }

        if (!isOnGround_) {
            UpdateJumpPoseAnimation();
        }

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

    case MovementState::Warping:
        UpdateWarping();
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

        ridingPlatformDelta_ = { 0.0f, 0.0f, 0.0f };
    }
    // エイム中はプレイヤーの正面をカメラへ合わせる。
    // ただし、薙ぎ払い攻撃中は攻撃方向がブレないように向きを固定する。
    const bool freezeFacing =
        freezeFacingWhileTongueSweeping_ &&
        tongue_ &&
        tongue_->IsSweeping();

    if (isAimMode && !freezeFacing) {
        SetYawFromCamera(cameraYaw);
    }

	if(!suppressTongueShotThisFrame_ &&
	   moveState_ != MovementState::CeilingCrawling &&
	   moveState_ != MovementState::WallClinging &&
	   moveState_ != MovementState::Warping &&
	   (input_->IsTriggerMouse(0) || input_->GetRightTrigger() >= 0.5f)) {
		Vector3 shotDirection = cameraForward;

        if (hasAimTargetPoint_ && tongue_) {
            Vector3 mouthPos = tongue_->GetMouthWorldPositionPublic();
            Vector3 toAim = aimTargetPoint_ - mouthPos;

            float lenSq = toAim.x * toAim.x + toAim.y * toAim.y + toAim.z * toAim.z;
            if (lenSq > 1e-8f) {
                float invLen = 1.0f / std::sqrt(lenSq);
                shotDirection = { toAim.x * invLen, toAim.y * invLen, toAim.z * invLen };
            }
        }

        TryShotTongue(shotDirection);
    }

	// B key: perform tongue-beam sweep (扇状薙ぎ)
	if ((input_->IsTriggerKey(DIK_E) || input_->IsPressPad(XINPUT_GAMEPAD_B)) && (moveState_ == MovementState::Root || moveState_ == MovementState::Jumping)) {
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
	if ((input_->IsTriggerKey(DIK_Q) || input_->IsPressPad(XINPUT_GAMEPAD_X)) && moveState_ != MovementState::Warping) {
		if (isMimicking_) {
			EndMimic();
		} else {
            abilityActive_ = !abilityActive_;
		}
	}

	// F key: activate sonar immediately
	if ((input_->IsTriggerKey(DIK_F) || input_->IsPressPad(XINPUT_GAMEPAD_Y)) && moveState_ != MovementState::Warping) {
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

    // ビーム攻撃のクールダウンタイマーをフレーム単位で減少させる
    if (beamActiveTimer_ > 0.0f) {
        float progress = 1.0f - (beamActiveTimer_ / beamActiveDuration_);
        float halfRad = beamActiveHalfAngleDeg_ * (3.14159265f / 180.0f) * 0.5f;
        float angle = -halfRad + progress * (2.0f * halfRad);

        Vector3 dir = beamActiveDir_;
        float s = std::sin(angle);
        float c = std::cos(angle);
        Vector3 sweepDir = { dir.x * c - dir.z * s, 0.0f, dir.x * s + dir.z * c };

        Vector3 origin = beamActiveOrigin_;
        origin.y += 0.5f; // same offset as TryUseBeam

        /// 舌の薙ぎ攻撃が有効な場合、ビームの到達距離と半径を舌の動きに完全に同期させる
        float dynamicRadius = beamOriginalCapsuleRadius_;
        float currentReach = beamActiveMaxRadius_;
        if (tongue_ && tongue_->IsSweeping()) {
            Vector3 mouth = tongue_->GetMouthWorldPositionPublic();
            Vector3 segEnd = tongue_->GetPosition();
            float dx = segEnd.x - mouth.x;
            float dy = segEnd.y - mouth.y;
            float dz = segEnd.z - mouth.z;
            float tipDist = std::sqrt(dx * dx + dy * dy + dz * dz);
            currentReach = tipDist;
            
            float tnorm = tipDist / std::max(1.0f, beamActiveMaxRadius_);
            dynamicRadius = beamOriginalCapsuleRadius_ * (1.0f + 0.6f * tnorm);
            if (enemyManager_) {
                enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
                    if (!e)
                        return;

                    for (const auto& part : e->GetTargetParts(0.7f)) {
                        bool alreadyHit = false;
                        for (auto h : beamActiveHitList_) {
                            if (h.first == e && h.second == part.partId) {
                                alreadyHit = true;
                                break;
                            }
                        }
                        if (alreadyHit)
                            continue;

                        Vector3 ep = part.position;
                        float distSq = PointToSegmentDistSq(ep, mouth, segEnd);
                        const float enemyRadius = 0.7f;
                        float hitRadius = dynamicRadius + enemyRadius;
                        if (distSq <= hitRadius * hitRadius) {
                            beamActiveHitList_.push_back({e, part.partId});
                            e->KillPart(part.partId);
                        }
                    }
                });
            }
        } else {
            if (progress < 0.5f) {
                float reachT = progress / 0.5f; // 0..1
                float reachMultiplier = 1.5f;
                currentReach = beamActiveMaxRadius_ * reachT * reachMultiplier;
                Vector3 segEnd = origin + sweepDir * currentReach;
                dynamicRadius = beamOriginalCapsuleRadius_ * (1.0f + 0.5f * reachT);
                if (enemyManager_) {
                    enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
                        if (!e)
                            return;

                        for (const auto& part : e->GetTargetParts(0.7f)) {
                            bool alreadyHit = false;
                            for (auto h : beamActiveHitList_) {
                                if (h.first == e && h.second == part.partId) {
                                    alreadyHit = true;
                                    break;
                                }
                            }
                            if (alreadyHit)
                                continue;

                            Vector3 ep = part.position;
                            float distSq = PointToSegmentDistSq(ep, origin, segEnd);
                            const float enemyRadius = 0.7f;
                            float hitRadius = dynamicRadius + enemyRadius;
                            if (distSq <= hitRadius * hitRadius) {
                                beamActiveHitList_.push_back({e, part.partId});
                                e->KillPart(part.partId);
                            }
                        }
                    });
                }
            } else {
                float returnT = (progress - 0.5f) / 0.5f; // 0..1
                currentReach = beamActiveMaxRadius_ * (1.0f - returnT);
                Vector3 segEnd = origin + sweepDir * currentReach;
                dynamicRadius = beamOriginalCapsuleRadius_ * (1.0f - 0.8f * returnT);
                //DebugRenderer::GetInstance()->AddLine(origin, segEnd, { 0.0f, 0.6f, 0.0f, 1.0f });
                if (enemyManager_) {
                    enemyManager_->ForEachEnemy([&](BaseEnemy* e) {
                        if (!e)
                            return;

                        for (const auto& part : e->GetTargetParts(0.7f)) {
                            bool alreadyHit = false;
                            for (auto h : beamActiveHitList_) {
                                if (h.first == e && h.second == part.partId) {
                                    alreadyHit = true;
                                    break;
                                }
                            }
                            if (alreadyHit)
                                continue;

                            Vector3 ep = part.position;
                            float distSq = PointToSegmentDistSq(ep, origin, segEnd);
                            const float enemyRadius = 0.7f;
                            float hitRadius = dynamicRadius + enemyRadius;
                            if (distSq <= hitRadius * hitRadius) {
                                beamActiveHitList_.push_back({e, part.partId});
                                e->KillPart(part.partId);
                            }
                        }
                    });
                }
            }
        }

        // ビーム攻撃状態が続いている間は、毎フレームこのように扇状の当たり判定をサンプリングして敵をヒットさせる
        beamActiveTimer_ = std::max(0.0f, beamActiveTimer_ - 1.0f);
    }

    if (enemyContactInvincibilityTimer_ > 0.0f) {
        enemyContactInvincibilityTimer_ =
            std::max(0.0f, enemyContactInvincibilityTimer_ - 1.0f);
    }

    UpdateHPGaugeSprite();
    UpdateJumpGaugeSprite();
    UpdateWallClingGaugeUISprite();
    UpdateAbilityLevelUI();

    if (enemyManager_) {
        speedMultiplier_ = enemyManager_->GetTotalPlayerSpeedMultiplier();
    } else {
        speedMultiplier_ = 1.0f;
    }
}

void Player::Draw()
{
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

    bool isAimMode = false;
    if (cameraController_) {
        isAimMode = cameraController_->IsAimMode();
    }

	float currentYaw = object_->GetRotate().y - modelYawOffset_;

    const bool freezeFacing =
        freezeFacingWhileTongueSweeping_ &&
        tongue_ &&
        tongue_->IsSweeping();

    if (LengthXZ(inputDir) > 0.0001f) {
        if (isAimMode) {
            // エイム中は正面をカメラへ固定したまま移動する。
            // ただし、正面方向と移動入力方向がズレているほど速度を下げる。
            Vector3 forward = {
                std::sin(currentYaw),
                0.0f,
                std::cos(currentYaw)
            };

            float alignment = forward.x * inputDir.x + forward.z * inputDir.z;
            alignment = std::clamp(alignment, 0.0f, 1.0f);

            float aimMoveRate =
                aimMoveMinSpeedRate_ +
                (1.0f - aimMoveMinSpeedRate_) * alignment;

            float actualSpeed = moveSpeed_ * speedMultiplier_ * aimMoveRate;

            Vector3 targetVelocity = {
                inputDir.x * actualSpeed,
                0.0f,
                inputDir.z * actualSpeed
            };

            groundMoveVelocity_.x = ApproachFloat(groundMoveVelocity_.x, targetVelocity.x, groundAcceleration_);
            groundMoveVelocity_.z = ApproachFloat(groundMoveVelocity_.z, targetVelocity.z, groundAcceleration_);
        }
        else {
            float desiredYaw = std::atan2(inputDir.x, inputDir.z);
            float yawDiff = WrapAnglePi(desiredYaw - currentYaw);

            if (!freezeFacing) {
                float yawStep = std::clamp(yawDiff, -turnSpeedRad_, turnSpeedRad_);
                currentYaw += yawStep;

                object_->SetRotate({ 0.0f, currentYaw + modelYawOffset_, 0.0f });
            }

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
		chargeTimer_ += 2.5f;

        int currentLevel = GetCurrentChargeLevel();
        int allowedLevel = GetAllowedChargeLevel();

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

int Player::GetCurrentChargeLevel() const
{
    if (chargeTimer_ >= chargeThresholds_[2])
        return 3;
    if (chargeTimer_ >= chargeThresholds_[1])
        return 2;
    if (chargeTimer_ >= chargeThresholds_[0])
        return 1;
    return 0;
}

int Player::GetAllowedChargeLevel() const { return std::clamp(chargeStock_, 0, kMaxChargeLevel_); }

void Player::ExecuteChargedJump(int chargeLevel)
{
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
    jumpChargeAirCancelGraceTimer_ = 0;

    if (chargeLevel > 0) {
        // chargeStock_ -= chargeLevel;
        if (chargeStock_ < 0) {
            chargeStock_ = 0;
        }
    }
}

void Player::CancelJumpCharge()
{
    isJumpChargeCanceled_ = true;
    isChargingJump_ = false;
    chargeTimer_ = 0.0f;
    chargeMaxHoldTimer_ = 0.0f;
    jumpChargeAirCancelGraceTimer_ = 0;
}

void Player::ApplyGravity()
{
    Vector3 position = object_->GetTranslate();

    isOnGround_ = false;

    velocity_.y += gravity_;

    // 下方向だけ制限
    velocity_.y = std::max(velocity_.y, -maxFallSpeed_);

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

void Player::UpdateAirborneHorizontalMove(float cameraYaw) {
    Vector3 position = object_->GetTranslate();

    Vector3 inputDir = GetMoveInputDirection(cameraYaw);

    Vector3 horizontalVelocity = {
        lockedJumpMoveVelocity_.x,
        0.0f,
        lockedJumpMoveVelocity_.z
    };

    float currentSpeed = LengthXZ(horizontalVelocity);

    if (LengthXZ(inputDir) > 0.0001f) {
        float alignment = 0.0f;

        if (currentSpeed > 0.0001f) {
            Vector3 currentDir = {
                horizontalVelocity.x / currentSpeed,
                0.0f,
                horizontalVelocity.z / currentSpeed
            };

            alignment = Vector3::Dot(currentDir, inputDir);
            alignment = ClampFloat(alignment, 0.0f, 1.0f);
        }
        else {
            alignment = 1.0f;
        }

        float accel = airControlAccelerationMin_ +
            (airControlAccelerationMax_ - airControlAccelerationMin_) * alignment;

        horizontalVelocity.x += inputDir.x * accel;
        horizontalVelocity.z += inputDir.z * accel;
    }
    else {
        float speed = LengthXZ(horizontalVelocity);
        if (speed > 0.0001f) {
            float newSpeed = std::max(0.0f, speed - airControlDrag_);
            float scale = newSpeed / speed;
            horizontalVelocity.x *= scale;
            horizontalVelocity.z *= scale;
        }
    }

    float newSpeed = LengthXZ(horizontalVelocity);
    if (newSpeed > airControlMaxSpeed_ && newSpeed > 0.0001f) {
        float scale = airControlMaxSpeed_ / newSpeed;
        horizontalVelocity.x *= scale;
        horizontalVelocity.z *= scale;
    }

    lockedJumpMoveVelocity_.x = horizontalVelocity.x;
    lockedJumpMoveVelocity_.z = horizontalVelocity.z;

    position.x += lockedJumpMoveVelocity_.x;
    position.z += lockedJumpMoveVelocity_.z;

    velocity_.x = lockedJumpMoveVelocity_.x;
    velocity_.z = lockedJumpMoveVelocity_.z;

    object_->SetTranslate(position);
}
const char* Player::GetMovementStateName() const
{
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
    case MovementState::Warping:
        return "Warping";
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
        EndJumpPoseAnimation();
        ClearClingStageObjectTracking();
        break;

    case MovementState::Jumping:
        isOnGround_ = false;
        lockedJumpMoveVelocity_.x = velocity_.x;
        lockedJumpMoveVelocity_.z = velocity_.z;
        ClearClingStageObjectTracking();

        StartJumpPoseAnimation();
        break;

	case MovementState::WallClinging:
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		break;

	case MovementState::TonguePulling:
        EndJumpPoseAnimation();
        break;

	case MovementState::CeilingCrawling:
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		break;

	case MovementState::Warping:
		velocity_ = { 0.0f, 0.0f, 0.0f };
		groundMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		lockedJumpMoveVelocity_ = { 0.0f, 0.0f, 0.0f };
		break;
	}
}

void Player::DrawImGui()
{
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Player")) {
        Vector3 position = GetPosition();
        Vector3 rotate = GetRotate();

        if (ImGui::TreeNode("Position / Velocity")) {
            ImGui::Text("World Position");
            ImGui::Separator();
            if (ImGui::DragFloat3("Position", &position.x, 0.01f)) {
                SetPendingTeleport(position);
            }

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
                    for (const auto& kv : abilityStates_) {
                        AbilityId id = kv.first;
                        const AbilityState& s = kv.second;

                        float need = XPToNextLevel(s.level);
                        float progress = (need > 0.0f) ? (s.xp / need) : 0.0f;
                        if (progress < 0.0f)
                            progress = 0.0f;
                        if (progress > 1.0f)
                            progress = 1.0f;

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
                    for (const auto& p : pendingAbilityXP_) {
                        ImGui::Text("%s : %.1f", AbilityIdToString(p.first), p.second);
                    }
                }

                ImGui::Separator();
                ImGui::Text("Pending LevelUps:");
                if (pendingLevelUps_.empty()) {
                    ImGui::Text("(none)");
                } else {
                    for (const auto& lv : pendingLevelUps_) {
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
                        if (ImGui::InputFloat(buf, &baseJumpPowers_[i], 0.0f, 0.0f, "%.3f"))
                            anyChanged = true;
                    }
                    if (ImGui::InputFloat("Jump perLevel", &jumpPowerPerLevel_, 0.0f, 0.0f, "%.3f"))
                        anyChanged = true;
                } else if (id == AbilityId::TongueRange) {
                    if (ImGui::InputFloat("Tongue Base Distance", &baseTongueMaxDistance_, 0.0f, 0.0f, "%.2f"))
                        anyChanged = true;
                    if (ImGui::InputFloat("Tongue perLevel", &tongueRangePerLevel_, 0.0f, 0.0f, "%.3f"))
                        anyChanged = true;
                } else if (id == AbilityId::SonarDuration) {
                    if (ImGui::InputFloat("Sonar Base (s)", &baseSonarDuration_, 0.0f, 0.0f, "%.2f"))
                        anyChanged = true;
                    if (ImGui::InputFloat("Sonar perLevel", &sonarDurationPerLevel_, 0.0f, 0.0f, "%.3f"))
                        anyChanged = true;
                } else if (id == AbilityId::WallClingDuration) {
                    if (ImGui::InputFloat("WallCling Base", &baseWallClingGauge_, 0.0f, 0.0f, "%.1f"))
                        anyChanged = true;
                    if (ImGui::InputFloat("WallCling perLevel", &wallClingPerLevel_, 0.0f, 0.0f, "%.3f"))
                        anyChanged = true;
                } else if (id == AbilityId::CamouflageDuration) {
                    if (ImGui::InputFloat("Camouflage Base (frames)", &baseCamouflageDuration_, 0.0f, 0.0f, "%.0f"))
                        anyChanged = true;
                    if (ImGui::InputFloat("Camouflage perLevel", &camouflagePerLevel_, 0.0f, 0.0f, "%.3f"))
                        anyChanged = true;
                }
                ImGui::Separator();
            }

            // 常に Apply Config を表示し、デザイナーがいつでも編集を反映できるようにする
            if (ImGui::Button("Apply Config")) {
                // apply config to runtime values based on current levels
                for (const auto& kv : abilityStates_) {
                    AbilityId id = kv.first;
                    int level = kv.second.level;
                    float mult = 1.0f + ((id == AbilityId::JumpPower) ? jumpPowerPerLevel_ : (id == AbilityId::TongueRange) ? tongueRangePerLevel_
                                                : (id == AbilityId::SonarDuration)                                          ? sonarDurationPerLevel_
                                                : (id == AbilityId::WallClingDuration)                                      ? wallClingPerLevel_
                                                                                                                            : camouflagePerLevel_)
                            * static_cast<float>(level - 1);
                    switch (id) {
                    case AbilityId::JumpPower:
                        jumpPowerMultiplier_ = mult;
                        break;
                    case AbilityId::TongueRange:
                        tongueRangeMultiplier_ = mult;
                        if (tongue_)
                            tongue_->SetMaxDistance(baseTongueMaxDistance_ * tongueRangeMultiplier_);
                        break;
                    case AbilityId::SonarDuration:
                        sonarDurationMultiplier_ = mult;
                        sonarDuration_ = baseSonarDuration_ * sonarDurationMultiplier_;
                        break;
                    case AbilityId::WallClingDuration:
                        wallClingDurationMultiplier_ = mult;
                        maxWallClingGauge_ = baseWallClingGauge_ * wallClingDurationMultiplier_;
                        if (wallClingGauge_ > maxWallClingGauge_)
                            wallClingGauge_ = maxWallClingGauge_;
                        break;
                    case AbilityId::CamouflageDuration:
                        camouflageDurationMultiplier_ = mult;
                        break;
                    default:
                        break;
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
            if (tongue_)
                tongueEffective = tongue_->GetMaxDistance();
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
                    Vector3 debugDirection = { 0.0f, 0.0f, 1.0f };
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
float Player::GetJumpChargeRate() const
{
    int allowedLevel = GetAllowedChargeLevel();
    if (allowedLevel <= 0)
        return 0.0f;

    float maxTime = (allowedLevel == 1) ? chargeThresholds_[0] : (allowedLevel == 2) ? chargeThresholds_[1]
                                                                                     : chargeThresholds_[2];

    float t = chargeTimer_ / maxTime;
    return std::clamp(t, 0.0f, 1.0f);
}

int Player::GetCurrentVisibleChargeLevel() const { return std::min(GetCurrentChargeLevel(), GetAllowedChargeLevel()); }

void Player::AddChargeStock(int amount)
{
    chargeStock_ += amount;
    chargeStock_ = std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::SetChargeStock(int stock) { chargeStock_ = std::clamp(stock, 0, kMaxChargeLevel_); }

int Player::GetCurrentChargePhase() const
{
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

float Player::GetCurrentChargePhaseRate() const
{
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

float Player::GetYaw() const
{
    if (!object_) {
        return 0.0f;
    }
    return object_->GetRotate().y;
}

void Player::UpdateWarping()
{
    if (!object_) return;

    warpTimer_ += 1.0f;
    float t = warpTimer_ / warpDuration_;

    if (t >= 1.0f) {
        t = 1.0f;
        object_->SetTranslate(warpTargetPosition_);
        TransitionTo(MovementState::Jumping);
    }
    else {
        // 線形補間
        Vector3 currentPos;
        currentPos.x = warpStartPosition_.x + (warpTargetPosition_.x - warpStartPosition_.x) * t;
        currentPos.y = warpStartPosition_.y + (warpTargetPosition_.y - warpStartPosition_.y) * t;
        currentPos.z = warpStartPosition_.z + (warpTargetPosition_.z - warpStartPosition_.z) * t;
        object_->SetTranslate(currentPos);
    }
}

void Player::UpdateWallClinging(float cameraYaw)
{
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

void Player::UpdateCeilingCrawling()
{
    if (!object_ || !hasClingSurface_) {
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

        if (canTransition) {
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

void Player::UpdateTransparencyByCamera(const Vector3& cameraPosition)
{
    if (!object_) {
        return;
    }

    Vector3 toPlayer = { GetPosition().x - cameraPosition.x, GetPosition().y - cameraPosition.y, GetPosition().z - cameraPosition.z };

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

void Player::SetupClingSurfaceFromHit(const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& hitNormal)
{
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
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
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

bool Player::IsInsideCurrentClingSurface(const Vector3& position) const
{
    if (!hasClingSurface_) {
        return false;
    }

    Vector3 toPos = position - clingSurfaceCenter_;
    float rightDist = Dot3(toPos, clingSurfaceRight_);
    float upDist = Dot3(toPos, clingSurfaceUp_);

    return std::abs(rightDist) <= (clingSurfaceHalfWidth_ + wallDetachMargin_) && std::abs(upDist) <= (clingSurfaceHalfHeight_ + wallDetachMargin_);
}

Vector3 Player::ClampPositionToCurrentClingSurface(const Vector3& position) const
{
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

void Player::ResolveCurrentClingPenetration(Vector3& position) const
{
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

void Player::ResolveWallClingBlockCollisions(Vector3& position) const
{
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

bool Player::TryReattachToAdjacentSurface(const Vector3& fromPosition, const Vector3& moveDir, Vector3& outPosition)
{
    if (!blockColliders_) {
        return false;
    }

    // 進行方向へ少し先の位置を基準に、近い面を探す
    Vector3 probePos = fromPosition + moveDir * clingReattachSearchDistance_;

    float bestScore = -1.0e9f;
    bool found = false;
    CollisionUtility::OBB bestBlock = {};
    Vector3 bestNormal = { 0.0f, 0.0f, 0.0f };

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

const char* Player::DebugFaceNameFromNormal(const CollisionUtility::OBB& block, const Vector3& normal) const
{
    Vector3 n = Normalize3(normal);

    struct FaceInfo {
        Vector3 normal;
        const char* name;
    };

    FaceInfo faces[6] = {
        { Normalize3(block.axis[0]), "+Axis0" },
        { Normalize3(block.axis[0]) * -1, "-Axis0" },
        { Normalize3(block.axis[1]), "+Axis1" },
        { Normalize3(block.axis[1]) * -1, "-Axis1" },
        { Normalize3(block.axis[2]), "+Axis2" },
        { Normalize3(block.axis[2]) * -1, "-Axis2" },
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

void Player::ResetTongueHitDebug()
{
    hasTongueHitDebug_ = false;
    debugTongueHitStep_ = -1;
    debugTongueHitPoint_ = { 0.0f, 0.0f, 0.0f };
    debugTongueRawNormal_ = { 0.0f, 0.0f, 0.0f };
    debugTongueUsedNormal_ = { 0.0f, 0.0f, 0.0f };
    debugTongueRawFaceName_ = "None";
    debugTongueUsedFaceName_ = "None";
}

void Player::RecordTongueHitDebug(int step, const CollisionUtility::OBB& block, const Vector3& hitPoint, const Vector3& rawNormal, const Vector3& usedNormal)
{
    hasTongueHitDebug_ = true;
    debugTongueHitStep_ = step;
    debugTongueHitPoint_ = hitPoint;
    debugTongueRawNormal_ = Normalize3(rawNormal);
    debugTongueUsedNormal_ = Normalize3(usedNormal);
    debugTongueRawFaceName_ = DebugFaceNameFromNormal(block, debugTongueRawNormal_);
    debugTongueUsedFaceName_ = DebugFaceNameFromNormal(block, debugTongueUsedNormal_);
}

void Player::RefreshClingAnchorFromCurrentSurface()
{
    if (!hasClingSurface_ || !tongue_) {
        return;
    }

    Vector3 clingAnchorPoint = clingSurfaceCenter_
        + clingSurfaceRight_ * clingHitRightOffset_
        + clingSurfaceUp_ * clingHitUpOffset_;

    Vector3 hookPos = clingAnchorPoint + clingSurfaceNormal_ * tongueHookSurfaceOffset_;

    // 状態を Hooked に巻き戻さない
    tongue_->SetHookPositionPreserveState(hookPos);

    CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());
    const float kPullSkin = 0.03f;
    float playerRadiusAlongNormal = ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_);

    tonguePullTarget_ = clingAnchorPoint
        + clingSurfaceNormal_ * (playerRadiusAlongNormal + kPullSkin);
}

void Player::ClearClingStageObjectTracking()
{
    clingStageObjectId_ = -1;
    clingStageObjectIsMovingPlatform_ = false;
}

bool Player::IsPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin) const
{
    if (!movementLimitCylinder_) {
        return true;
    }

    // プレイヤーのXZ半径を保守的に見積もる
    float playerRadiusXZ = std::max(colliderHalfSize_.x, colliderHalfSize_.z) + extraMargin;
    float usableRadius = movementLimitCylinder_->radius - playerRadiusXZ;

    if (usableRadius <= 0.0f) {
        return false;
    }

    float dx = position.x - movementLimitCylinder_->center.x;
    float dz = position.z - movementLimitCylinder_->center.z;
    float distSq = dx * dx + dz * dz;

    if (distSq > usableRadius * usableRadius) {
        return false;
    }

    float minY = movementLimitCylinder_->center.y - movementLimitCylinder_->halfHeight + colliderHalfSize_.y;
    float maxY = movementLimitCylinder_->center.y + movementLimitCylinder_->halfHeight - colliderHalfSize_.y;

    return position.y >= minY && position.y <= maxY;
}

Vector3 Player::ClampPlayerPositionInsideMovementCylinder(const Vector3& position, float extraMargin) const
{
    if (!movementLimitCylinder_) {
        return position;
    }

    Vector3 clamped = position;

    float playerRadiusXZ = std::max(colliderHalfSize_.x, colliderHalfSize_.z) + extraMargin;
    float usableRadius = movementLimitCylinder_->radius - playerRadiusXZ;

    if (usableRadius > 0.0f) {
        float dx = clamped.x - movementLimitCylinder_->center.x;
        float dz = clamped.z - movementLimitCylinder_->center.z;
        float lenSq = dx * dx + dz * dz;

        if (lenSq > usableRadius * usableRadius && lenSq > 1e-8f) {
            float len = std::sqrt(lenSq);
            float scale = usableRadius / len;
            clamped.x = movementLimitCylinder_->center.x + dx * scale;
            clamped.z = movementLimitCylinder_->center.z + dz * scale;
        }
    } else {
        clamped.x = movementLimitCylinder_->center.x;
        clamped.z = movementLimitCylinder_->center.z;
    }

    float minY = movementLimitCylinder_->center.y - movementLimitCylinder_->halfHeight + colliderHalfSize_.y;
    float maxY = movementLimitCylinder_->center.y + movementLimitCylinder_->halfHeight - colliderHalfSize_.y;
    clamped.y = ClampFloat(clamped.y, minY, maxY);

    return clamped;
}

void Player::UpdateJumpGaugeSprite()
{
    if (!camera_ || !jumpGaugeBackSprite_ || !jumpGaugeFillSprite_) {
        return;
    }

    showJumpGauge_ = isChargingJump_;
    if (!showJumpGauge_) {
        return;
    }

    Vector3 worldPos = GetPosition();
    worldPos.y += 2.0f;

    const float screenW = static_cast<float>(WinApp::kClientWidth);
    const float screenH = static_cast<float>(WinApp::kClientHeight);

    Vector2 screenPos{};
    if (!WorldToScreenSimple(
        worldPos,
        camera_->GetViewProjectionMatrix(),
        screenW,
        screenH,
        screenPos)) {
        // 投影失敗時も消さず、画面上部中央へ逃がす
        screenPos.x = screenW * 0.5f;
        screenPos.y = screenH * 0.20f;
    }

    Vector2 gaugePos = {
        screenPos.x + jumpGaugeOffset_.x,
        screenPos.y + jumpGaugeOffset_.y
    };

    const float kScreenMargin = 8.0f;

    gaugePos.x = ClampFloat(
        gaugePos.x,
        kScreenMargin,
        screenW - jumpGaugeSize_.x - kScreenMargin);

    gaugePos.y = ClampFloat(
        gaugePos.y,
        kScreenMargin,
        screenH - jumpGaugeSize_.y - kScreenMargin);

    jumpGaugeBackSprite_->SetPos(gaugePos);
    jumpGaugeBackSprite_->SetSize(jumpGaugeSize_);
    jumpGaugeBackSprite_->Update();

    int allowedLevel = std::max(1, GetAllowedChargeLevel());
    int visibleLevel = GetCurrentVisibleChargeLevel();
    float phaseRate = ClampFloat(GetCurrentChargePhaseRate(), 0.0f, 1.0f);

    Vector4 segmentColors[4] = {
        { 0.25f, 1.0f, 0.2f, 0.9f },
        { 0.95f, 0.95f, 0.2f, 0.9f },
        { 1.0f, 0.6f, 0.2f, 0.9f },
        { 1.0f, 0.25f, 0.25f, 0.9f }
    };

    Vector4 gaugeColor = segmentColors[std::clamp(visibleLevel, 0, 3)];
    jumpGaugeFillSprite_->SetColor(gaugeColor);

    float totalWidth = jumpGaugeSize_.x;
    float fillRate = 0.0f;

    if (allowedLevel > 0) {
        fillRate = (static_cast<float>(visibleLevel) + phaseRate) / static_cast<float>(allowedLevel);
    }
    fillRate = ClampFloat(fillRate, 0.0f, 1.0f);

    jumpGaugeFillSprite_->SetPos(gaugePos);
    jumpGaugeFillSprite_->SetSize({ totalWidth * fillRate, jumpGaugeSize_.y });
    jumpGaugeFillSprite_->Update();
}

void Player::UpdateAbilityLevelUI()
{

    abilityLevelUIBlinkTimer_ += 1.0f / 60.0f;

    if (!showAbilityLevelUI_) {
        return;
    }

    if (abilityLevelUIEntries_.empty()) {
        return;
    }

    const float screenW = static_cast<float>(WinApp::kClientWidth);
    const float screenH = static_cast<float>(WinApp::kClientHeight);

    const float blockHeight =
        abilityLevelUIIconSize_.y +
        abilityLevelUILevelTextOffset_.y +
        abilityLevelUINumberSize_.y;

    const float totalHeight =
        blockHeight * static_cast<float>(abilityLevelUIEntries_.size()) +
        abilityLevelUIVerticalGap_ * static_cast<float>(abilityLevelUIEntries_.size() - 1);

    const float startX = screenW - abilityLevelUIRightMargin_ - abilityLevelUIIconSize_.x;
    const float startY = screenH - abilityLevelUIBottomMargin_ - totalHeight;

    for (size_t i = 0; i < abilityLevelUIEntries_.size(); ++i) {
        auto& entry = abilityLevelUIEntries_[i];

        float x = startX;
        float y = startY + static_cast<float>(i) * (blockHeight + abilityLevelUIVerticalGap_);

        int level = 1;
        int maxLevel = 10;
        float xp = 0.0f;

        auto it = abilityStates_.find(entry.ability);
        if (it != abilityStates_.end()) {
            level = it->second.level;
            maxLevel = it->second.maxLevel;
            xp = it->second.xp;
        }

        float xpRate = 0.0f;
        if (level >= maxLevel) {
            xpRate = 1.0f;
        }
        else {
            float need = XPToNextLevel(level);
            if (need > 0.0001f) {
                xpRate = ClampFloat(xp / need, 0.0f, 1.0f);
            }
        }

        // アイコン
        if (entry.iconSprite) {
            entry.iconSprite->SetPos({ x, y });
            entry.iconSprite->SetSize(abilityLevelUIIconSize_);
            entry.iconSprite->Update();
        }

        if (entry.stateOverlaySprite) {
            Vector4 overlayColor = { 1.0f, 1.0f, 1.0f, 0.0f };

            const bool isMimicAbilityOn =
                entry.ability == AbilityId::CamouflageDuration &&
                currentAbility_ == Ability::Camouflage &&
                abilityActive_ &&
                !isMimicking_;

            if (isMimicAbilityOn) {
                float blink = 0.5f + 0.5f * std::sin(abilityLevelUIBlinkTimer_ * 8.0f);

                overlayColor = mimicAbilityOnColor_;
                overlayColor.w *= 0.55f + 0.45f * blink;
            }

            const float expand = mimicAbilityOnOverlayExpand_;

            entry.stateOverlaySprite->SetPos({
                x - expand * 0.5f,
                y - expand * 0.5f
                });
            entry.stateOverlaySprite->SetSize({
                abilityLevelUIIconSize_.x + expand,
                abilityLevelUIIconSize_.y + expand
                });
            entry.stateOverlaySprite->SetColor(overlayColor);
            entry.stateOverlaySprite->Update();
        }

        // XP進捗の半透明矩形
        if (entry.xpFillSprite) {
            float fillHeight = abilityLevelUIIconSize_.y * xpRate;

            entry.xpFillSprite->SetPos({
                x,
                y + (abilityLevelUIIconSize_.y - fillHeight)
                });
            entry.xpFillSprite->SetSize({
                abilityLevelUIIconSize_.x,
                fillHeight
                });
            entry.xpFillSprite->SetColor(abilityLevelUIXPFillColor_);
            entry.xpFillSprite->Update();
        }

        // "lv."
        if (entry.lvPrefixSprite) {
            entry.lvPrefixSprite->SetPos({
                x,
                y + abilityLevelUILevelTextOffset_.y
                });
            entry.lvPrefixSprite->SetSize(abilityLevelUILvPrefixSize_);
            entry.lvPrefixSprite->Update();
        }

        // 数字
        entry.levelNumberText.SetPosition({
            x + abilityLevelUILvPrefixSize_.x + 2.0f,
            y + abilityLevelUILevelTextOffset_.y - 1.0f
            });
        entry.levelNumberText.SetNumber(level, 1);
        entry.levelNumberText.Update();
    }
}

void Player::DrawAbilityLevelUI()
{
    if (!showAbilityLevelUI_) {
        return;
    }

    for (auto& entry : abilityLevelUIEntries_) {
        if (entry.stateOverlaySprite) {
            entry.stateOverlaySprite->Draw();
        }
        if (entry.iconSprite) {
            entry.iconSprite->Draw();
        }
        if (entry.xpFillSprite) {
            entry.xpFillSprite->Draw();
        }
        if (entry.lvPrefixSprite) {
            entry.lvPrefixSprite->Draw();
        }
        entry.levelNumberText.Draw();
    }
}

void Player::UpdateHPGaugeSprite()
{
    if (!hpGaugeBackSprite_ || !hpGaugeFillSprite_ || !hpGaugeFrameSprite_) {
        return;
    }

    float hpRate = 1.0f;
    if (maxHp_ > 0) {
        hpRate = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    }
    hpRate = ClampFloat(hpRate, 0.0f, 1.0f);

    hpGaugeBackSprite_->SetPos(hpGaugePos_);
    hpGaugeBackSprite_->SetSize(hpGaugeSize_);
    hpGaugeBackSprite_->Update();

    hpGaugeFrameSprite_->SetPos({
        hpGaugePos_.x - hpGaugeFrameThickness_,
        hpGaugePos_.y - hpGaugeFrameThickness_
        });
    hpGaugeFrameSprite_->SetSize({
        hpGaugeSize_.x + hpGaugeFrameThickness_ * 2.0f,
        hpGaugeSize_.y + hpGaugeFrameThickness_ * 2.0f
        });
    hpGaugeFrameSprite_->Update();

    Vector4 hpColor = { 0.85f, 0.15f, 0.15f, 0.95f };
    if (hpRate <= 0.5f) {
        hpColor = { 0.95f, 0.75f, 0.15f, 0.95f };
    }
    if (hpRate <= 0.25f) {
        hpColor = { 0.95f, 0.25f, 0.10f, 0.98f };
    }

    hpGaugeFillSprite_->SetPos(hpGaugePos_);
    hpGaugeFillSprite_->SetSize({
        hpGaugeSize_.x * hpRate,
        hpGaugeSize_.y
        });
    hpGaugeFillSprite_->SetColor(hpColor);
    hpGaugeFillSprite_->Update();
}

void Player::UpdateWallClingGaugeUISprite()
{
    if (!wallClingGaugeBackSprite_ || !wallClingGaugeFillSprite_) {
        return;
    }

    float rate = 1.0f;
    if (maxWallClingGauge_ > 0.0001f) {
        rate = wallClingGauge_ / maxWallClingGauge_;
    }
    rate = ClampFloat(rate, 0.0f, 1.0f);

    wallClingGaugeBackSprite_->SetPos(wallClingGaugePos_);
    wallClingGaugeBackSprite_->SetSize(wallClingGaugeSize_);
    wallClingGaugeBackSprite_->Update();

    Vector4 staminaColor = { 0.95f, 0.85f, 0.15f, 0.95f };
    if (rate <= 0.5f) {
        staminaColor = { 1.0f, 0.70f, 0.10f, 0.95f };
    }
    if (rate <= 0.25f) {
        staminaColor = { 1.0f, 0.45f, 0.10f, 0.98f };
    }

    wallClingGaugeFillSprite_->SetPos(wallClingGaugePos_);
    wallClingGaugeFillSprite_->SetSize({
        wallClingGaugeSize_.x * rate,
        wallClingGaugeSize_.y
        });
    wallClingGaugeFillSprite_->SetColor(staminaColor);
    wallClingGaugeFillSprite_->Update();
}

void Player::DrawUI()
{
    if (hpGaugeFrameSprite_) {
        hpGaugeFrameSprite_->Draw();
    }
    if (hpGaugeBackSprite_) {
        hpGaugeBackSprite_->Draw();
    }
    if (hpGaugeFillSprite_) {
        hpGaugeFillSprite_->Draw();
    }

    // 壁のぼりスタミナ
    if (showWallClingGaugeUI_) {
        if (wallClingGaugeBackSprite_) {
            wallClingGaugeBackSprite_->Draw();
        }
        if (wallClingGaugeFillSprite_) {
            wallClingGaugeFillSprite_->Draw();
        }
    }

    // ジャンプゲージは表示時だけ
    if (showJumpGauge_) {
        if (jumpGaugeBackSprite_) {
            jumpGaugeBackSprite_->Draw();
        }
        if (jumpGaugeFillSprite_) {
            jumpGaugeFillSprite_->Draw();
        }
    }

    DrawAbilityLevelUI();
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

void Player::StartJumpPoseAnimation()
{
    if (!object_ || !useWalkFrameAsJumpPose_) {
        return;
    }

    // 歩き中からジャンプしても、ジャンプポーズ用に0フレームから取り直す
    object_->SetAnimationPause(false);
    object_->PlayAnimation(jumpPoseAnimationName_, false, 0.0f);
    object_->RestartAnimation();

    jumpPoseAnimationActive_ = true;
}

void Player::UpdateJumpPoseAnimation()
{
    if (!object_ || !jumpPoseAnimationActive_) {
        return;
    }

    int holdFrame = jumpPoseHoldFrame_;

    const int totalFrames = object_->GetAnimationTotalFrames();
    if (totalFrames > 0) {
        holdFrame = std::clamp(holdFrame, 0, totalFrames - 1);
    }

    if (object_->GetAnimationCurrentFrame() >= holdFrame) {
        object_->SetAnimationPause(true);
        jumpPoseAnimationActive_ = false;
    }
}

void Player::EndJumpPoseAnimation()
{
    jumpPoseAnimationActive_ = false;

    if (!object_) {
        return;
    }

    object_->SetAnimationPause(false);
}

void Player::UpdateClingStageObjectFromHitPoint(const Vector3& hitPoint)
{
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

        CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t, { 1.0f, 1.0f, 1.0f });

        auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(probe, obb);
        if (!hit.hit) {
            continue;
        }

        clingStageObjectId_ = o.id;
        clingStageObjectIsMovingPlatform_ = true;
        return;
    }
}

void Player::RefreshMovingClingSurfaceFromStage()
{
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

        CollisionUtility::OBB newObb = CollisionUtility::MakeOBBFromTransform(t, { 1.0f, 1.0f, 1.0f });

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