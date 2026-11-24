#include "enemy/RammerEnemy.h"

Enemy::BehaviorResult RammerEnemy::UpdateBehavior(const EnemyTypeParams& params, const Context& context) {
	// 体当たりOFFの間は、その場に浮いている的になる(接近の予告もしない)
	if (!context.ramEnabled) {
		Hold();
		return {};
	}

	// 判定半径の和より内側まで詰めるので、近づき切れば必ず体当たりになる
	Approach(params, context.playerPosition, context.deltaTime);

	BehaviorResult result;
	// 接近の予告は移動後の距離で判定する(間合いに入ったフレームで鳴る)
	result.approached = CheckApproach(params, context.playerPosition);
	return result;
}

bool RammerEnemy::CheckApproach(const EnemyTypeParams& params, const Vector3& playerPosition) {
	// 予告済みの個体と、予告しない設定(範囲0)は飛ばす
	if (approachAlerted_ || params.approachSoundRange <= 0.0f) {
		return false;
	}
	const float distanceSquared = (playerPosition - HitCenter(params)).LengthSquared();
	if (distanceSquared > params.approachSoundRange * params.approachSoundRange) {
		return false;
	}
	// 範囲に入った時点で予告済みにする。音が鳴らせなかった(他の敵の音が鳴っていた)個体も、後から遅れては鳴らさない
	// (遅れて鳴ると、どの敵が近づいた合図なのか分からなくなるため)
	approachAlerted_ = true;
	return true;
}
