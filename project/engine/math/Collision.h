#pragma once
#include "math/Vector3.h"

#include <algorithm>

// 衝突判定の形状と交差判定。
// ゲーム固有の概念(属性ビットマスク・衝突応答など)は持たない純粋な形状計算なので、
// CatmullRomSplineと同じくengine/math/へ置く。判定側の都合はapplication/が持つこと。
// 現状は「ステージ地形(AABB) vs プレイヤー(球)」に必要な分のみ。
// 総当たりで足りる規模なので空間分割はしない。

// 軸平行境界ボックス(Axis-Aligned Bounding Box)。
// 「軸平行」なので回転を表現できない。回転が必要になったらOBBを別途用意すること
struct AABB {
	Vector3 min = { 0.0f, 0.0f, 0.0f };
	Vector3 max = { 0.0f, 0.0f, 0.0f };
};

/// <summary>
/// AABBと球の交差判定
/// </summary>
/// <param name="aabb">判定対象のAABB(ワールド空間)</param>
/// <param name="sphereCenter">球の中心(ワールド空間)</param>
/// <param name="sphereRadius">球の半径</param>
/// <returns>交差していれば true</returns>
inline bool IsCollision(const AABB& aabb, const Vector3& sphereCenter, float sphereRadius) {
	// 球の中心をAABBの内側へクランプすると、AABB上で球に最も近い点(最近接点)が得られる。
	// 中心がAABB内にある場合はクランプされず距離0になり、そのまま交差と判定できる
	const Vector3 closest = {
		std::clamp(sphereCenter.x, aabb.min.x, aabb.max.x),
		std::clamp(sphereCenter.y, aabb.min.y, aabb.max.y),
		std::clamp(sphereCenter.z, aabb.min.z, aabb.max.z),
	};

	// 平方根を避けるため距離の二乗で比較する
	const Vector3 diff = closest - sphereCenter;
	return diff.LengthSquared() <= sphereRadius * sphereRadius;
}

/// <summary>
/// 球同士の交差判定(自弾と敵の判定に使う)
/// </summary>
/// <param name="centerA">球Aの中心(ワールド空間)</param>
/// <param name="radiusA">球Aの半径</param>
/// <param name="centerB">球Bの中心(ワールド空間)</param>
/// <param name="radiusB">球Bの半径</param>
/// <returns>交差していれば true</returns>
inline bool IsCollision(const Vector3& centerA, float radiusA, const Vector3& centerB, float radiusB) {
	// 中心間距離が半径の和以下なら交差。AABB版と同じく平方根を避けて二乗で比較する
	const Vector3 diff = centerB - centerA;
	const float radiusSum = radiusA + radiusB;
	return diff.LengthSquared() <= radiusSum * radiusSum;
}
