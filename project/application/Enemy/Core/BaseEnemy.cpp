#include "BaseEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include "../../../application/Player.h"
#include <algorithm>
#include <fstream>
#include <../externals/nlohmann/json.hpp>
#include <random>

BaseEnemy::BaseEnemy() = default;
BaseEnemy::~BaseEnemy() = default;

void BaseEnemy::ResolveHorizontalCollisions(const Vector3& previousPosition)
{
    // プレイヤーと同じ安定サイズ（1.0f）で壁判定を行う
    ResolveHorizontalCollisionsForPos(position_, previousPosition, 1.0f);
}

void BaseEnemy::ResolveVerticalCollisions()
{
    // プレイヤー（Player.cpp 423行目）と同様の計算
    // 判定半径を 1.0f（クッション込）、見た目の高さを 0.5f（モデルの中心）として処理
    ResolveVerticalCollisionsForPos(position_, velocity_, 1.0f, 0.5f, isOnGround_);
}

void BaseEnemy::ResolveHorizontalCollisionsForPos(Vector3& pos, const Vector3& prevPos, float radius) const
{
    if (!blockColliders_)
        return;
    const float kGroundEpsilon = 0.05f;

    if (pos.x != prevPos.x) {
        CollisionUtility::OBB testObb = GetOBB({ pos.x, prevPos.y + kGroundEpsilon, pos.z }, radius);
        for (const auto& block : *blockColliders_) {
            if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
                pos.x = prevPos.x;
                break;
            }
        }
    }
    if (pos.z != prevPos.z) {
        CollisionUtility::OBB testObb = GetOBB({ pos.x, prevPos.y + kGroundEpsilon, pos.z }, radius);
        for (const auto& block : *blockColliders_) {
            if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
                pos.z = prevPos.z;
                break;
            }
        }
    }
}

void BaseEnemy::ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround) const
{
    outOnGround = false;
    const float kSkinWidth = 0.005f;

    if (blockColliders_) {
        for (const auto& block : *blockColliders_) {
            CollisionUtility::OBB myObb = GetOBB(pos, collisionRadius);
            auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(myObb, block);
            if (!hit.hit)
                continue;

            // プレイヤーと同じ -= normal で押し出す
            pos -= hit.normal * (hit.penetration + kSkinWidth);

            if (hit.normal.y < -0.5f) { // 床
                if (vel.y < 0.0f)
                    vel.y = 0.0f;
                outOnGround = true;
            }
            if (hit.normal.y > 0.5f && vel.y > 0.0f) { // 天井
                vel.y = 0.0f;
            }
        }
    }

    // 【重要】地面リミットの強制補正
    // プレイヤーは Y=1.0 付近で立っているため、それに合わせて visualRadius を加味した floorLimit を計算
    float floorLimit = groundY_ + collisionRadius; // 1.0 に合わせる
    if (pos.y < floorLimit) {
        pos.y = floorLimit;
        if (vel.y < 0.0f)
            vel.y = 0.0f;
        outOnGround = true;
    }
}

void BaseEnemy::SetColor(const Vector4& color)
{
    if (object_)
        object_->SetColor(color);
    originalColor_ = color;
}

void BaseEnemy::SetAlpha(float a)
{
    if (object_) {
        Vector4 c = originalColor_;
        c.w = a;
        object_->SetColor(c);
    }
}

float BaseEnemy::GetOriginalAlpha() const
{
    return originalColor_.w;
}

CollisionUtility::OBB BaseEnemy::GetOBB(const Vector3& pos, float radius) const
{
    Transform t;
    t.translate = pos;
    // const関数内なので object_ の非constメソッド呼び出しに注意（適宜修正してください）
    t.rotate = object_ ? object_->GetRotate() : Vector3 { 0, 0, 0 };
    return CollisionUtility::MakeOBBFromTransform(t, { radius, radius, radius });
}

// ドロップ配布の実装
void BaseEnemy::DistributeDrops() {
    if (dropTable_.empty() || !player_)
        return;

    std::vector<float> weights;
    std::vector<const DropEntry*> entries;
    std::random_device rd;
    std::mt19937 gen(rd());

    for (const auto& d : dropTable_) {
        std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
        if (d.chance >= 1.0f || chanceDist(gen) <= d.chance) {
            float w = d.weight > 0.0f ? d.weight : 0.0f;
            weights.push_back(w);
            entries.push_back(&d);
        }
    }

    if (entries.empty())
        return;

    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    size_t idx = dist(gen);
    const DropEntry* chosen = entries[idx];
    if (!chosen)
        return;

    int amount = chosen->minAmount;
    if (chosen->maxAmount > chosen->minAmount) {
        std::uniform_int_distribution<int> amtDist(chosen->minAmount, chosen->maxAmount);
        amount = amtDist(gen);
    }

    // プレイヤーに能力XPを付与
    if (player_) {
        player_->EnqueueAbilityXP(chosen->ability, static_cast<float>(amount));
    }
}