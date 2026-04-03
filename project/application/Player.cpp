#include "Player.h"

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include "Enemy/Core/BaseEnemy.h"
#include "CameraController.h"
#include "Enemy/Manager/EnemyManager.h"
#include "utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>

Player::~Player() = default;

namespace {
float LengthXZ(const Vector3& v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

float Length3(const Vector3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

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

float Dot3(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float ComputeOBBSupportRadiusAlongNormal(const CollisionUtility::OBB& obb, const Vector3& normal)
{
    return std::abs(Dot3(obb.axis[0], normal)) * obb.halfLength[0] + std::abs(Dot3(obb.axis[1], normal)) * obb.halfLength[1] + std::abs(Dot3(obb.axis[2], normal)) * obb.halfLength[2];
}

float AbsDot3(const Vector3& a, const Vector3& b)
{
    return std::abs(Dot3(a, b));
}

Vector3 Cross3(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float ClampFloat(float v, float minV, float maxV)
{
    return std::max(minV, std::min(v, maxV));
}

void Player::Initialize(
    Object3dCommon* object3dCommon,
    Camera* camera,
    const std::string& modelName,
    const Vector3& startPosition)
{
    camera_ = camera;
    input_ = Input::GetInstance();

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(modelName);
    object_->SetCamera(camera_);
    object_->SetTranslate(startPosition);
    object_->SetRotate({ 0.0f, 0.0f, 0.0f });

    tongue_ = std::make_unique<Tongue>();
    tongue_->Initialize(object3dCommon, camera_, this, "Cube.obj");

    velocity_ = { 0.0f, 0.0f, 0.0f };
    lastMove_ = { 0.0f, 0.0f, 0.0f };
    isOnGround_ = false;

    waterGauge_ = maxWaterGauge_;

    isChargingJump_ = false;
    isJumpChargeCanceled_ = false;
    chargeTimer_ = 0.0f;
    chargeMaxHoldTimer_ = 0.0f;
    moveState_ = MovementState::Root;
    wallClingGauge_ = maxWallClingGauge_;
    prevAimMode_ = false;
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
    Vector3 toCenter = {
        movementLimitCylinder_->center.x - position.x,
        0.0f,
        movementLimitCylinder_->center.z - position.z
    };

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

bool Player::CanStartTongueShot() const{
    if(!tongue_){
        return false;
    }

    // すでに舌が発射状態なら不可
    // Idle 以外は Extending / Hooked / Returning を含めて全部禁止
    if(tongue_->GetState() != Tongue::State::Idle){
        return false;
    }



    // 舌で引っ張られている間や壁張り付き中も止めたいならここで追加できる
     if(moveState_ == MovementState::TonguePulling || moveState_ == MovementState::WallClinging ||
        moveState_ == MovementState::CeilingCrawling){ return false; }

    return true;
}

bool Player::IsGroundSurface(const Vector3& normal) const{
    return normal.y >= clingGroundNormalThreshold_;
}

bool Player::IsCeilingSurface(const Vector3& normal) const{
    return normal.y <= -clingCeilingNormalThreshold_;
}

bool Player::IsWallSurface(const Vector3& normal) const{
    return !IsGroundSurface(normal) && !IsCeilingSurface(normal);
}

Vector3 Player::ResolveHookSurfaceNormal(
    const CollisionUtility::OBB& block,
    const Vector3& hitPoint,
    const Vector3& tongueDelta,
    const Vector3& playerPos) const{
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

    struct Candidate{
        Vector3 normal;
        float distance;
    };

    Candidate c[6] = {
        { axis[0],      std::abs(block.halfLength[0] - local[0]) },
        { axis[0] * -1, std::abs(-block.halfLength[0] - local[0]) },
        { axis[1],      std::abs(block.halfLength[1] - local[1]) },
        { axis[1] * -1, std::abs(-block.halfLength[1] - local[1]) },
        { axis[2],      std::abs(block.halfLength[2] - local[2]) },
        { axis[2] * -1, std::abs(-block.halfLength[2] - local[2]) },
    };

    int nearest = 0;
    int second = 1;
    if(c[second].distance < c[nearest].distance){
        std::swap(nearest, second);
    }

    for(int i = 2; i < 6; ++i){
        if(c[i].distance < c[nearest].distance){
            second = nearest;
            nearest = i;
        } else if(c[i].distance < c[second].distance){
            second = i;
        }
    }

    Vector3 positionBasedNormal = c[nearest].normal;
    float ambiguousMargin = c[second].distance - c[nearest].distance;

    Vector3 travelDir = Normalize3(tongueDelta);
    Vector3 directionBasedNormal = positionBasedNormal;
    {
        float bestScore = -1.0e9f;
        for(int i = 0; i < 3; ++i){
            Vector3 a = axis[i];
            Vector3 b = axis[i] * -1.0f;

            float scoreA = Dot3(a, travelDir * -1.0f);
            if(scoreA > bestScore){
                bestScore = scoreA;
                directionBasedNormal = a;
            }

            float scoreB = Dot3(b, travelDir * -1.0f);
            if(scoreB > bestScore){
                bestScore = scoreB;
                directionBasedNormal = b;
            }
        }
    }

    Vector3 toPlayer = Normalize3(playerPos - hitPoint);
    Vector3 playerBasedNormal = positionBasedNormal;
    {
        float bestScore = -1.0e9f;
        for(int i = 0; i < 3; ++i){
            Vector3 a = axis[i];
            Vector3 b = axis[i] * -1.0f;

            float scoreA = Dot3(a, toPlayer);
            if(scoreA > bestScore){
                bestScore = scoreA;
                playerBasedNormal = a;
            }

            float scoreB = Dot3(b, toPlayer);
            if(scoreB > bestScore){
                bestScore = scoreB;
                playerBasedNormal = b;
            }
        }
    }

    Vector3 resolved = positionBasedNormal;

    const float kAmbiguousThreshold = 0.05f;
    if(ambiguousMargin < kAmbiguousThreshold){
        resolved = directionBasedNormal;
    }

    if(IsWallSurface(positionBasedNormal) && IsGroundSurface(directionBasedNormal)){
        resolved = directionBasedNormal;
    }

    if(Dot3(resolved, directionBasedNormal) < 0.5f &&
       Dot3(playerBasedNormal, directionBasedNormal) > 0.5f){
        resolved = directionBasedNormal;
    }

    return Normalize3(resolved);
}

Vector3 Player::ResolveHookSurfaceNormalFromPlayerCapsule(
    const CollisionUtility::OBB& block,
    const Vector3& playerPos,
    const Vector3& hitPoint,
    const Vector3& tongueDelta) const{
    Vector3 axis[3] = {
        Normalize3(block.axis[0]),
        Normalize3(block.axis[1]),
        Normalize3(block.axis[2]),
    };

    Vector3 segmentDir = Normalize3(hitPoint - playerPos);
    Vector3 travelDir = Normalize3(tongueDelta);

    Vector3 bestNormal = { 0.0f, 1.0f, 0.0f };
    float bestScore = -1.0e9f;

    // 線分の中点も使って、どの面側に近いかを見る
    Vector3 midPoint = {
        (playerPos.x + hitPoint.x) * 0.5f,
        (playerPos.y + hitPoint.y) * 0.5f,
        (playerPos.z + hitPoint.z) * 0.5f
    };

    Vector3 localMidVec = midPoint - block.center;
    float localMid[3] = {
        Dot3(localMidVec, axis[0]),
        Dot3(localMidVec, axis[1]),
        Dot3(localMidVec, axis[2]),
    };

    for(int i = 0; i < 3; ++i){
        Vector3 candidates[2] = {
            axis[i],
            axis[i] * -1.0f
        };

        for(int j = 0; j < 2; ++j){
            Vector3 n = candidates[j];

            // プレイヤー→hitPoint の進行に対して入口面らしいか
            float frontScore = Dot3(n, segmentDir * -1.0f);

            // 舌の進行方向ともある程度整合しているか
            float tongueScore = Dot3(n, travelDir * -1.0f);

            // 線分中点がその面寄りにあるか
            float sideScore = 0.0f;
            if(i == 0){
                sideScore = (j == 0) ? localMid[0] : -localMid[0];
            } else if(i == 1){
                sideScore = (j == 0) ? localMid[1] : -localMid[1];
            } else{
                sideScore = (j == 0) ? localMid[2] : -localMid[2];
            }

            // 合成スコア
            float score =
                frontScore * 0.60f +
                tongueScore * 0.25f +
                sideScore * 0.15f;

            // 下方向ショットで壁面を選びにくくする
            if(segmentDir.y < -0.4f && IsWallSurface(n)){
                score -= 0.5f;
            }

            if(score > bestScore){
                bestScore = score;
                bestNormal = n;
            }
        }
    }

    return Normalize3(bestNormal);
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

void Player::SetRidingPlatformDelta(const Vector3& delta)
{
    ridingPlatformDelta_ = delta;
}

void Player::ClearRidingPlatformDelta()
{
    ridingPlatformDelta_ = { 0.0f, 0.0f, 0.0f };
}

Vector3 Player::GetPosition() const
{
    if (!object_) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return object_->GetTranslate();
}

Vector3 Player::GetRotate() const
{ 
    if(!object_) {
        return { 0.0f, 0.0f, 0.0f };
	}
    return object_->GetRotate(); };

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

bool Player::TryShotTongue(const Vector3& direction){
    if(!CanStartTongueShot()){
        return false;
    }

    if(!ConsumeWater(tongueWaterCost_)){
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

    if (position.x != previousPosition.x) {
        Vector3 testPos = {
            position.x,
            previousPosition.y + kGroundEpsilon,
            previousPosition.z
        };

        CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

        for (const auto& block : *blockColliders_) {
            if (CollisionUtility::IntersectOBB_OBB(playerObb, block)) {
                position.x = previousPosition.x;
                break;
            }
        }
    }

    if (position.z != previousPosition.z) {
        Vector3 testPos = {
            position.x,
            previousPosition.y + kGroundEpsilon,
            position.z
        };

        CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

        for (const auto& block : *blockColliders_) {
            if (CollisionUtility::IntersectOBB_OBB(playerObb, block)) {
                position.z = previousPosition.z;
                break;
            }
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

        if (!hit.hit) {
            continue;
        }

        const float kSkinWidth = 0.001f;
        position -= hit.normal * (hit.penetration + kSkinWidth);

        if (hit.normal.y < -0.5f) {
            velocity_.y = 0.0f;
            isOnGround_ = true;
        }
        if (hit.normal.y > 0.5f && velocity_.y > 0.0f) {
            velocity_.y = 0.0f;
        }
    }

    object_->SetTranslate(position);
}

void Player::CheckTongueBlockHook(){
    if(!tongue_ || !blockColliders_){
        return;
    }

    if(tongue_->GetState() != Tongue::State::Extending){
        return;
    }

    ResetTongueHitDebug();

    Vector3 start = tongue_->GetPrevPosition();
    Vector3 end = tongue_->GetPosition();
    Vector3 delta = end - start;
    float moveLen = Length3(delta);

    CollisionUtility::Sphere baseSphere = tongue_->GetHitSphere();

    // 半径より大きく飛ぶなら分割数を増やす
    int steps = std::max(1, static_cast<int>(std::ceil(moveLen / std::max(0.05f, baseSphere.radius * 0.5f))));

    for(int s = 1; s <= steps; ++s){
        float t = static_cast<float>(s) / static_cast<float>(steps);

        CollisionUtility::Sphere testSphere = baseSphere;
        testSphere.center = start + delta * t;

        for(const auto& block : *blockColliders_){
            auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(testSphere, block);
            if(!hit.hit){
                continue;
            }

            Vector3 rawHitNormal = Normalize3(hit.normal);

            Vector3 correctedHitNormal = ResolveHookSurfaceNormalFromPlayerCapsule(
                block,
                GetPosition(),
                hit.point,
                delta);

            Vector3 usedHitNormal = debugIgnoreHookSurfaceCorrection_
                ? rawHitNormal
                : correctedHitNormal;

            RecordTongueHitDebug(
                s,
                block,
                hit.point,
                rawHitNormal,
                usedHitNormal);

            if(std::strcmp(debugTongueUsedFaceName_, "+Axis1") == 0){
                tongue_->StartReturn();
                return;
            }

            // raw確認中は地面除外も無視できるようにする
            if(!debugIgnoreGroundRejectOnRawHit_){
                if(IsGroundSurface(usedHitNormal)){
                    continue;
                }
            }

            SetupClingSurfaceFromHit(block, hit.point, usedHitNormal);

            Vector3 hookPos = hit.point + clingSurfaceNormal_ * tongueHookSurfaceOffset_;
            tongue_->SetHooked(hookPos);

            CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());
            const float kPullSkin = 0.03f;
            float playerRadiusAlongNormal = ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_);

            Vector3 clingAnchorPoint =
                clingSurfaceCenter_
                + clingSurfaceRight_ * clingHitRightOffset_
                + clingSurfaceUp_ * clingHitUpOffset_;

            tonguePullTarget_ = clingAnchorPoint + clingSurfaceNormal_ * (playerRadiusAlongNormal + kPullSkin);

            velocity_ = { 0.0f, 0.0f, 0.0f };
            CancelJumpCharge();

            if(useTonguePull_){
                TransitionTo(MovementState::TonguePulling);
            } else{
                tongue_->StartReturn();
            }
            return;
        }
    }
}

void Player::UpdateTonguePulling()
{
    if (!object_ || !tongue_) {
        return;
    }

    Vector3 position = object_->GetTranslate();
    Vector3 toTarget = tonguePullTarget_ - position;
    float distance = Length3(toTarget);

    if (distance <= tonguePullEndDistance_) {
        Vector3 snapped = tonguePullTarget_;
        if (hasClingSurface_) {
            snapped = ClampPositionToCurrentClingSurface(snapped);
            ResolveCurrentClingPenetration(snapped);
        }

        object_->SetTranslate(snapped);
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isOnGround_ = false;
        tongue_->StartReturn();
        TransitionTo(MovementState::WallClinging);
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

    float cameraYaw = 0.0f;
    bool isAimMode = false;
    Vector3 cameraForward = { 0.0f, 0.0f, 1.0f };

    if (cameraController_) {
        cameraYaw = cameraController_->GetYaw();
        isAimMode = cameraController_->IsAimMode();
        cameraForward = cameraController_->GetForwardDirection();
    }

    if (pendingTeleport_) {
        object_->SetTranslate(pendingTeleportPosition_);
        velocity_ = { 0.0f, 0.0f, 0.0f };
        TransitionTo(MovementState::Jumping);
        pendingTeleport_ = false;
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

        Logger::Log(std::string("Player applied riding delta early posAfter:") + std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z) + " delta:" + std::to_string(ridingPlatformDelta_.x) + "," + std::to_string(ridingPlatformDelta_.y) + "," + std::to_string(ridingPlatformDelta_.z) + "\n");

        ridingPlatformDelta_ = { 0.0f, 0.0f, 0.0f };
    }

    switch (moveState_) {
    case MovementState::Root:
    case MovementState::Jumping: {
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
        } else {
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

	if(moveState_ != MovementState::CeilingCrawling &&moveState_ != MovementState::WallClinging && input_->IsTriggerMouse(0)){
		Vector3 shotDirection = cameraForward;

		if(hasAimTargetPoint_ && tongue_){
			Vector3 mouthPos = tongue_->GetMouthWorldPositionPublic();
			Vector3 toAim = aimTargetPoint_ - mouthPos;

			float lenSq = toAim.x * toAim.x + toAim.y * toAim.y + toAim.z * toAim.z;
			if(lenSq > 1e-8f){
				float invLen = 1.0f / std::sqrt(lenSq);
				shotDirection = {
					toAim.x * invLen,
					toAim.y * invLen,
					toAim.z * invLen
				};
			}
		}

		TryShotTongue(shotDirection);
	}

    // B key: perform tongue-beam sweep (扇状薙ぎ)
    if (input_->IsTriggerKey(DIK_B)) {
        // Use player's facing direction (yaw) for the sweep so arc is relative to player forward
        float yawForward = GetYaw();
        Vector3 beamDir = { std::sin(yawForward), 0.0f, std::cos(yawForward) };
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
        CheckTongueBlockHook();
    }

    // Update ability timers (frame-based)
    if (beamTimer_ > 0.0f)
        beamTimer_ = std::max(0.0f, beamTimer_ - 1.0f);
}

void Player::Draw()
{
    if (tongue_) {
        tongue_->Draw();
    }
    if (object_) {
        object_->Update();
        object_->SetColor({ 1.0f, 1.0f, 1.0f, currentAlpha_ });
        object_->Draw();
    }
}

void Player::MoveHorizontal(float cameraYaw)
{
    Vector3 position = object_->GetTranslate();
    lastMove_ = { 0.0f, 0.0f, 0.0f };

    Vector3 forward = { std::sin(cameraYaw), 0.0f, std::cos(cameraYaw) };
    Vector3 right = { std::cos(cameraYaw), 0.0f, -std::sin(cameraYaw) };

    if (input_->IsPushKey(DIK_W)) {
        lastMove_.x += forward.x;
        lastMove_.z += forward.z;
    }
    if (input_->IsPushKey(DIK_S)) {
        lastMove_.x -= forward.x;
        lastMove_.z -= forward.z;
    }
    if (input_->IsPushKey(DIK_D)) {
        lastMove_.x += right.x;
        lastMove_.z += right.z;
    }
    if (input_->IsPushKey(DIK_A)) {
        lastMove_.x -= right.x;
        lastMove_.z -= right.z;
    }

    if (input_->IsPushKey(DIK_R)) {
        position = { 0.0f, resetHeight_, 0.0f };
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isOnGround_ = false;
        CancelJumpCharge();
        if (tongue_) {
            tongue_->Reset();
        }
        TransitionTo(MovementState::Jumping);
        object_->SetTranslate(position);
        return;
    }

    float length = LengthXZ(lastMove_);
    if (length > 0.0f) {
        lastMove_.x /= length;
        lastMove_.z /= length;

        position.x += lastMove_.x * moveSpeed_;
        position.z += lastMove_.z * moveSpeed_;

        float yaw = std::atan2(lastMove_.x, lastMove_.z) + modelYawOffset_;
        object_->SetRotate({ 0.0f, yaw, 0.0f });
    }

    object_->SetTranslate(position);
}

void Player::UpdateJumpCharge()
{
    if (!IsOnGround()) {
        isChargingJump_ = false;
        chargeTimer_ = 0.0f;
        chargeMaxHoldTimer_ = 0.0f;
        isJumpChargeCanceled_ = false;
        return;
    }

    if (input_->IsTriggerKey(DIK_SPACE)) {
        isChargingJump_ = true;
        isJumpChargeCanceled_ = false;
        chargeTimer_ = 0.0f;
        chargeMaxHoldTimer_ = 0.0f;
    }

    if (!isChargingJump_) {
        return;
    }

    if (input_->IsPushKey(DIK_SPACE)) {
        chargeTimer_ += 1.0f;

        int currentLevel = GetCurrentChargeLevel();
        int allowedLevel = GetAllowedChargeLevel();

        if (currentLevel >= allowedLevel) {
            chargeMaxHoldTimer_ += 1.0f;
            if (chargeMaxHoldTimer_ >= chargeCancelHoldLimit_) {
                CancelJumpCharge();
                return;
            }
        } else {
            chargeMaxHoldTimer_ = 0.0f;
        }
    }

    if (!input_->IsPushKey(DIK_SPACE)) {
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

int Player::GetAllowedChargeLevel() const
{
    return std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::ExecuteChargedJump(int chargeLevel)
{
    chargeLevel = std::clamp(chargeLevel, 0, kMaxChargeLevel_);

    velocity_.y = jumpPowers_[chargeLevel];
    isOnGround_ = false;

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
}

void Player::ApplyGravity()
{
    Vector3 position = object_->GetTranslate();

    isOnGround_ = false;

    velocity_.y += gravity_;
    position.y += velocity_.y;

    // 非常用の落下リセット
    if (position.y <= -50.0f) {
        position = { 0.0f, resetHeight_, 0.0f };
        velocity_ = { 0.0f, 0.0f, 0.0f };
        if (tongue_) {
            tongue_->Reset();
        }
    }

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
    }
    return "Unknown";
}

void Player::TransitionTo(MovementState nextState)
{
    if (moveState_ == nextState) {
        return;
    }

    if (moveState_ == MovementState::Root && nextState != MovementState::Root) {
        CancelJumpCharge();
    }

    moveState_ = nextState;

    switch (moveState_) {
    case MovementState::Root:
        velocity_.y = 0.0f;
        isOnGround_ = true;
        break;

    case MovementState::Jumping:
        isOnGround_ = false;
        break;

    case MovementState::WallClinging: {
        velocity_ = { 0.0f, 0.0f, 0.0f };
        wallClingGauge_ = maxWallClingGauge_;

        if (hasClingSurface_) {
            wallRightVec_ = clingSurfaceRight_;
        } else {
            wallRightVec_ = { 1.0f, 0.0f, 0.0f };
        }

        isOnGround_ = false;
        break;
    }

    case MovementState::TonguePulling:
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isOnGround_ = false;
        break;
    case MovementState::CeilingCrawling:
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isOnGround_ = false;
        break;
    }

}

void Player::DrawImGui(){
#ifdef USE_IMGUI
    if(ImGui::TreeNode("Player")){
        Vector3 position = GetPosition();
        Vector3 rotate = GetRotate();

        if(ImGui::TreeNode("Position / Velocity")){
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
        }

        if(ImGui::TreeNode("Water")){
            ImGui::Text("Water Gauge : %.1f / %.1f", waterGauge_, maxWaterGauge_);
            ImGui::ProgressBar(
                maxWaterGauge_ > 0.0f ? (waterGauge_ / maxWaterGauge_) : 0.0f,
                ImVec2(240.0f, 22.0f));
            ImGui::Text("Tongue Cost : %.1f", tongueWaterCost_);
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("State")){
            ImGui::Text("Current State : %s", GetMovementStateName());
            ImGui::Separator();

            if(ImGui::Button("To Root")){
                TransitionTo(MovementState::Root);
            }
            ImGui::SameLine();
            if(ImGui::Button("To Jumping")){
                TransitionTo(MovementState::Jumping);
            }
            ImGui::SameLine();
            if(ImGui::Button("To WallClinging")){
                TransitionTo(MovementState::WallClinging);
            }
            ImGui::SameLine();
            if(ImGui::Button("To TonguePulling")){
                TransitionTo(MovementState::TonguePulling);
            }

            ImGui::Separator();
            ImGui::Text("Wall Gauge");
            ImGui::ProgressBar(
                maxWallClingGauge_ > 0.0f ? (wallClingGauge_ / maxWallClingGauge_) : 0.0f,
                ImVec2(240.0f, 22.0f));
            ImGui::Text("%.1f / %.1f", wallClingGauge_, maxWallClingGauge_);
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("Jump")){
            ImGui::Text("Space : Hold to Charge Jump");
            ImGui::Separator();

            int stock = GetChargeStock();
            int phase = GetCurrentChargePhase();
            float phaseRate = GetCurrentChargePhaseRate();
            int visibleLevel = GetCurrentVisibleChargeLevel();

            ImGui::Text("Charge Stock : %d", stock);
            ImGui::Text("Current Level : %d", visibleLevel);

            int editableStock = stock;
            if(ImGui::SliderInt("Edit Charge Stock", &editableStock, 0, kMaxChargeLevel_)){
                SetChargeStock(editableStock);
                stock = editableStock;
            }

            if(ImGui::Button("+1 Stock")){
                AddChargeStock(1);
            }
            ImGui::SameLine();
            if(ImGui::Button("-1 Stock")){
                AddChargeStock(-1);
            }
            ImGui::SameLine();
            if(ImGui::Button("Max Stock")){
                SetChargeStock(kMaxChargeLevel_);
            }

            ImGui::Separator();

            if(stock <= 0){
                ImGui::Text("Phase : Normal Jump Only");
            } else{
                if(IsChargeAtMaxPhase()){
                    ImGui::Text("Phase : MAX (%d / %d)", visibleLevel, stock);
                } else{
                    ImGui::Text("Phase : %d / %d", phase + 1, stock);
                }
            }

            ImGui::ProgressBar(phaseRate, ImVec2(240.0f, 22.0f));
            ImGui::Text("Phase Charge : %d%%", static_cast<int>(phaseRate * 100.0f));

            if(IsChargeAtMaxPhase()){
                ImGui::Text("Holding too long will cancel jump");
            }

            ImGui::TreePop();
        }

        if(ImGui::TreeNode("Tongue")){
            if(tongue_){
                Vector3 tonguePos = tongue_->GetPosition();
                ImGui::Text("Position : %.3f %.3f %.3f", tonguePos.x, tonguePos.y, tonguePos.z);

                const char* stateName = "Idle";
                switch(tongue_->GetState()){
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

                if(ImGui::Button("Shot Tongue")){
                    Vector3 debugDirection = { 0.0f, 0.0f, 1.0f };
                    if(cameraController_){
                        debugDirection = cameraController_->GetForwardDirection();
                    }

                    TryShotTongue(debugDirection);
                }
                ImGui::SameLine();
                if(ImGui::Button("Reset Tongue")){
                    tongue_->Reset();
                    if(moveState_ == MovementState::TonguePulling){
                        TransitionTo(MovementState::Jumping);
                    }
                }
                ImGui::Checkbox("Use Tongue Pull", &useTonguePull_);

                ImGui::Separator();
                ImGui::Text("Tongue Hit Debug");
                ImGui::Checkbox("Show Raw Hit Debug", &debugShowRawTongueHit_);
                ImGui::Checkbox("Ignore Surface Correction", &debugIgnoreHookSurfaceCorrection_);
                ImGui::Checkbox("Ignore Ground Reject", &debugIgnoreGroundRejectOnRawHit_);

                if(ImGui::Button("Clear Hit Debug")){
                    ResetTongueHitDebug();
                }

                if(debugShowRawTongueHit_){
                    ImGui::Separator();
                    ImGui::Text("Has Hit Debug : %s", hasTongueHitDebug_ ? "true" : "false");
                    ImGui::Text("Hit Step : %d", debugTongueHitStep_);

                    ImGui::Text("Hit Point : %.3f %.3f %.3f",
                                debugTongueHitPoint_.x,
                                debugTongueHitPoint_.y,
                                debugTongueHitPoint_.z);

                    ImGui::Text("Raw Normal : %.3f %.3f %.3f",
                                debugTongueRawNormal_.x,
                                debugTongueRawNormal_.y,
                                debugTongueRawNormal_.z);
                    ImGui::Text("Raw Face : %s", debugTongueRawFaceName_);

                    ImGui::Text("Used Normal : %.3f %.3f %.3f",
                                debugTongueUsedNormal_.x,
                                debugTongueUsedNormal_.y,
                                debugTongueUsedNormal_.z);
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

    float maxTime = (allowedLevel == 1) ? chargeThresholds_[0]
        : (allowedLevel == 2)           ? chargeThresholds_[1]
                                        : chargeThresholds_[2];

    float t = chargeTimer_ / maxTime;
    return std::clamp(t, 0.0f, 1.0f);
}

int Player::GetCurrentVisibleChargeLevel() const
{
    return std::min(GetCurrentChargeLevel(), GetAllowedChargeLevel());
}

void Player::AddChargeStock(int amount)
{
    chargeStock_ += amount;
    chargeStock_ = std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::SetChargeStock(int stock)
{
    chargeStock_ = std::clamp(stock, 0, kMaxChargeLevel_);
}

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

bool Player::IsChargeAtMaxPhase() const
{
    return GetCurrentVisibleChargeLevel() >= GetAllowedChargeLevel() && GetAllowedChargeLevel() > 0 && GetCurrentChargePhaseRate() >= 1.0f;
}

float Player::GetYaw() const
{
    if (!object_) {
        return 0.0f;
    }
    return object_->GetRotate().y;
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

    if (input_->IsPushKey(DIK_W)) {
        moveVec += clingSurfaceUp_;
    }
    if (input_->IsPushKey(DIK_S)) {
        moveVec -= clingSurfaceUp_;
    }
    if (input_->IsPushKey(DIK_D)) {
        moveVec -= wallRightVec_;
    }
    if (input_->IsPushKey(DIK_A)) {
        moveVec += wallRightVec_;
    }

    float len = std::sqrt(moveVec.x * moveVec.x + moveVec.y * moveVec.y + moveVec.z * moveVec.z);
    if (len > 0.0001f) {
        moveVec = moveVec * (1.0f / len);
        position += moveVec * wallMoveSpeed_;
    }

    // 先に張り付いた面へ向きをそろえる
    float yaw = std::atan2(-clingSurfaceNormal_.x, -clingSurfaceNormal_.z) + modelYawOffset_;
    object_->SetRotate({ 0.0f, yaw, 0.0f });

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

    if(input_->IsTriggerKey(DIK_SPACE)){
        object_->SetTranslate(position);

        // 天井に張り付いているときだけ高速移動モードへ移行
        if(IsCeilingSurface(clingSurfaceNormal_)){
            TransitionTo(MovementState::CeilingCrawling);
            return;
        }

        // 壁のときは従来どおり離脱ジャンプ
        velocity_ = clingSurfaceNormal_ * 0.25f;
        velocity_.y = jumpPowers_[0];
        hasClingSurface_ = false;
        TransitionTo(MovementState::Jumping);
        return;
    }

    object_->SetTranslate(position);
}

void Player::UpdateCeilingCrawling(){
    if(!object_ || !hasClingSurface_){
        TransitionTo(MovementState::Jumping);
        return;
    }

    Vector3 position = object_->GetTranslate();

    // カメラ前方を現在の張り付き面へ射影して進行方向を作る
    Vector3 moveDir = clingSurfaceUp_;
    if(cameraController_){
        Vector3 cameraForward = cameraController_->GetForwardDirection();
        float dot = Dot3(cameraForward, clingSurfaceNormal_);
        moveDir = cameraForward - clingSurfaceNormal_ * dot;
    }

    float len = std::sqrt(
        moveDir.x * moveDir.x +
        moveDir.y * moveDir.y +
        moveDir.z * moveDir.z
    );

    if(len > 0.0001f){
        moveDir = moveDir * (1.0f / len);
    }

    Vector3 nextPosition = position + moveDir * ceilingCrawlSpeed_;

    // 端に出たかどうかは clamp 前の位置で判定する
    if(!IsInsideCurrentClingSurface(nextPosition)){
        Vector3 toNext = nextPosition - clingSurfaceCenter_;
        float rightDist = Dot3(toNext, clingSurfaceRight_);
        float upDist = Dot3(toNext, clingSurfaceUp_);

        Vector3 nextNormal = clingSurfaceNormal_;
        bool canTransition = false;

        // 天井面から同じブロックの側面へ移る
        if(IsCeilingSurface(clingSurfaceNormal_)){
            if(rightDist > clingSurfaceHalfWidth_){
                nextNormal = clingSurfaceRight_;
                canTransition = true;
            } else if(rightDist < -clingSurfaceHalfWidth_){
                nextNormal = clingSurfaceRight_ * -1.0f;
                canTransition = true;
            } else if(upDist > clingSurfaceHalfHeight_){
                nextNormal = clingSurfaceUp_;
                canTransition = true;
            } else if(upDist < -clingSurfaceHalfHeight_){
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

            object_->SetTranslate(reattachPos);

            float yaw = std::atan2(-clingSurfaceNormal_.x, -clingSurfaceNormal_.z) + modelYawOffset_;
            object_->SetRotate({ 0.0f, yaw, 0.0f });

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

    object_->SetTranslate(position);

    float yaw = std::atan2(moveDir.x, moveDir.z) + modelYawOffset_;
    object_->SetRotate({ 0.0f, yaw, 0.0f });

    // 左クリックで手を離す
    if(input_->IsTriggerMouse(0)){
        hasClingSurface_ = false;
        velocity_ = { 0.0f, 0.0f, 0.0f };
        suppressTongueShotThisFrame_ = true;
        TransitionTo(MovementState::Jumping);
        return;
    }

    // もう一度 Space を押したら通常の張り付き状態へ戻す
    if(input_->IsTriggerKey(DIK_SPACE)){
        TransitionTo(MovementState::WallClinging);
        return;
    }
}

void Player::UpdateTransparencyByCamera(const Vector3& cameraPosition)
{
    if (!object_) {
        return;
    }

    Vector3 toPlayer = {
        GetPosition().x - cameraPosition.x,
        GetPosition().y - cameraPosition.y,
        GetPosition().z - cameraPosition.z
    };

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

void Player::SetupClingSurfaceFromHit(
    const CollisionUtility::OBB& block,
    const Vector3& hitPoint,
    const Vector3& hitNormal)
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

bool Player::TryReattachToAdjacentSurface(const Vector3& fromPosition, const Vector3& moveDir, Vector3& outPosition){
    if(!blockColliders_){
        return false;
    }

    // 進行方向へ少し先の位置を基準に、近い面を探す
    Vector3 probePos = fromPosition + moveDir * clingReattachSearchDistance_;

    float bestScore = -1.0e9f;
    bool found = false;
    CollisionUtility::OBB bestBlock = {};
    Vector3 bestNormal = { 0.0f, 0.0f, 0.0f };

    for(const auto& block : *blockColliders_){
        CollisionUtility::OBB probeObb = GetPlayerOBB(probePos);
        auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(probeObb, block);
        if(!hit.hit){
            continue;
        }

        Vector3 hitNormal = hit.normal * -1.0f;

        // 地面は除外
        if(IsGroundSurface(hitNormal)){
            continue;
        }

        // 今いる面とほぼ同じ面も除外
        if(Dot3(hitNormal, clingSurfaceNormal_) > 0.95f){
            continue;
        }

        // 進行方向に対して自然につながる面を優先
        float score = Dot3(hitNormal, clingSurfaceNormal_ * -1.0f);

        if(score > bestScore){
            bestScore = score;
            bestBlock = block;
            bestNormal = hitNormal;
            found = true;
        }
    }

    if(!found){
        return false;
    }

    SetupClingSurfaceFromHit(bestBlock, probePos, bestNormal);

    outPosition = ClampPositionToCurrentClingSurface(probePos);
    ResolveCurrentClingPenetration(outPosition);
    ResolveWallClingBlockCollisions(outPosition);
    outPosition = ClampPositionToCurrentClingSurface(outPosition);

    return true;
}

const char* Player::DebugFaceNameFromNormal(
    const CollisionUtility::OBB& block,
    const Vector3& normal) const{
    Vector3 n = Normalize3(normal);

    struct FaceInfo{
        Vector3 normal;
        const char* name;
    };

    FaceInfo faces[6] = {
        { Normalize3(block.axis[0]),      "+Axis0" },
        { Normalize3(block.axis[0]) * -1, "-Axis0" },
        { Normalize3(block.axis[1]),      "+Axis1" },
        { Normalize3(block.axis[1]) * -1, "-Axis1" },
        { Normalize3(block.axis[2]),      "+Axis2" },
        { Normalize3(block.axis[2]) * -1, "-Axis2" },
    };

    float bestDot = -1.0f;
    const char* bestName = "Unknown";

    for(const auto& face : faces){
        float d = Dot3(n, face.normal);
        if(d > bestDot){
            bestDot = d;
            bestName = face.name;
        }
    }

    return bestName;
}

void Player::ResetTongueHitDebug(){
    hasTongueHitDebug_ = false;
    debugTongueHitStep_ = -1;
    debugTongueHitPoint_ = { 0.0f, 0.0f, 0.0f };
    debugTongueRawNormal_ = { 0.0f, 0.0f, 0.0f };
    debugTongueUsedNormal_ = { 0.0f, 0.0f, 0.0f };
    debugTongueRawFaceName_ = "None";
    debugTongueUsedFaceName_ = "None";
}

void Player::RecordTongueHitDebug(
    int step,
    const CollisionUtility::OBB& block,
    const Vector3& hitPoint,
    const Vector3& rawNormal,
    const Vector3& usedNormal){
    hasTongueHitDebug_ = true;
    debugTongueHitStep_ = step;
    debugTongueHitPoint_ = hitPoint;
    debugTongueRawNormal_ = Normalize3(rawNormal);
    debugTongueUsedNormal_ = Normalize3(usedNormal);
    debugTongueRawFaceName_ = DebugFaceNameFromNormal(block, debugTongueRawNormal_);
    debugTongueUsedFaceName_ = DebugFaceNameFromNormal(block, debugTongueUsedNormal_);
}