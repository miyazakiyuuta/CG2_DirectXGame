#include "BaseEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include <algorithm>

BaseEnemy::BaseEnemy() = default;
BaseEnemy::~BaseEnemy() = default;

void BaseEnemy::ResolveHorizontalCollisions(const Vector3& previousPosition) {
	// プレイヤーと同じ安定サイズ（1.0f）で壁判定を行う
	ResolveHorizontalCollisionsForPos(position_, previousPosition, 1.0f);
}

void BaseEnemy::ResolveVerticalCollisions() {
	// プレイヤー（Player.cpp 423行目）と同様の計算
	// 判定半径を 1.0f（クッション込）、見た目の高さを 0.5f（モデルの中心）として処理
	ResolveVerticalCollisionsForPos(position_, velocity_, 1.0f, 0.5f, isOnGround_);
}

void BaseEnemy::ResolveHorizontalCollisionsForPos(Vector3& pos, const Vector3& prevPos, float radius) const {
	if (!blockColliders_)
		return;
	const float kGroundEpsilon = 0.05f;

	if (pos.x != prevPos.x) {
		CollisionUtility::OBB testObb = GetOBB({pos.x, prevPos.y + kGroundEpsilon, pos.z}, radius);
		for (const auto& block : *blockColliders_) {
			if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
				pos.x = prevPos.x;
				break;
			}
		}
	}
	if (pos.z != prevPos.z) {
		CollisionUtility::OBB testObb = GetOBB({pos.x, prevPos.y + kGroundEpsilon, pos.z}, radius);
		for (const auto& block : *blockColliders_) {
			if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
				pos.z = prevPos.z;
				break;
			}
		}
	}
}

void BaseEnemy::ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround) const {
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

CollisionUtility::OBB BaseEnemy::GetOBB(const Vector3& pos, float radius) const {
	Transform t;
	t.translate = pos;
	// const関数内なので object_ の非constメソッド呼び出しに注意（適宜修正してください）
	t.rotate = object_ ? object_->GetRotate() : Vector3{0, 0, 0};
	return CollisionUtility::MakeOBBFromTransform(t, {radius, radius, radius});
}