#include "enemy/ShooterEnemy.h"

#include "bullet/BulletManager.h"
#include "utility/Random.h"

Enemy::BehaviorResult ShooterEnemy::UpdateBehavior(const EnemyTypeParams& params, const Context& context) {
	// 間合いまで近づいて止まる(keepDistanceが遠いので、手前で止まって撃つ形になる)。
	// 体当たりOFFでも射撃型は動く: 体当たりをしないので、止める理由がない
	Approach(params, context.playerPosition, context.deltaTime);

	BehaviorResult result;
	if (TryShoot(params, context)) {
		result.shotCount = 1;
	}
	return result;
}

void ShooterEnemy::OnSpawn(const EnemyTypeParams& params, const Context& context) {
	shootCooldown_ = NextShootCooldown(params, context.shootIntervalRandom);
}

bool ShooterEnemy::TryShoot(const EnemyTypeParams& params, const Context& context) {
	if (!context.enemyBulletManager) {
		return false; // 発射先が無ければ撃たない(体当たりだけの挙動に戻る)
	}
	// クールタイムは撃てない状況でも消化する(射程に入った瞬間に撃てる)
	shootCooldown_ -= context.deltaTime;
	if (!context.shootEnabled || shootCooldown_ > 0.0f) {
		return false;
	}

	// 撃つ位置は判定の中心(体の前寄り)。女王のように長い体の敵が、体の奥(重心)から弾を出さないようにする
	const Vector3 muzzle = HitCenter(params);
	const Vector3 toPlayer = context.playerPosition - muzzle;
	const float distanceSquared = toPlayer.LengthSquared();
	if (distanceSquared > params.shootRange * params.shootRange) {
		return false; // 遠すぎる。クールタイムは減ったままにして、近づいたら撃たせる
	}
	// 自機の後ろへ回った敵は撃たない。背後からの弾は画面の外から飛んでくるので、
	// 避ける手立てがないまま当たる(射撃型は「見えている位置から撃つ」ことが成立条件)
	if (Vector3::Dot(toPlayer, context.playerForward) > 0.0f) {
		return false;
	}

	shootCooldown_ = NextShootCooldown(params, context.shootIntervalRandom);
	// 狙いは「撃った瞬間の自機の位置」。先読みはしない:
	// 自機は毎フレーム動かせるので、置き弾の方が避けて当てる遊びとして成立する
	context.enemyBulletManager->Fire(muzzle, toPlayer);
	return true;
}

float ShooterEnemy::NextShootCooldown(const EnemyTypeParams& params, float intervalRandom) {
	// 間隔に±intervalRandomの幅を掛ける。
	// 同時に出た敵が同じ周期で撃ち続けると、避けようのない斉射になるため個体差を付ける
	const float low = params.shootInterval * (1.0f - intervalRandom);
	const float high = params.shootInterval * (1.0f + intervalRandom);
	return Random::GetFloat(low, high);
}
