#pragma once
#include "enemy/Enemy.h"

/// <summary>
/// 体当たり型の敵(sword)。撃たずに一気に間合いを詰め、自機に触れること自体を攻撃にする
/// (keepDistanceを判定半径の和より内側にしてあるので、近づき切れば必ず触れる)。
/// 近づいたら接近音で予告し、視界の端から突っ込んでくる敵にも身構えられるようにする
/// </summary>
class RammerEnemy : public Enemy {
public:
	BehaviorResult UpdateBehavior(const EnemyTypeParams& params, const Context& context) override;

private:
	// 接近音の範囲に入ったか。入った最初のフレームだけtrueを返す(1体につき1回だけ予告する)
	bool CheckApproach(const EnemyTypeParams& params, const Vector3& playerPosition);

	// 接近音を予告済みか(近くに居続けても鳴り直さない)。
	// 体当たり型にしか要らない状態なので、基底(Enemy)ではなくここに置く
	bool approachAlerted_ = false;
};
