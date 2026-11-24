#pragma once
#include "enemy/Enemy.h"

/// <summary>
/// 射撃型の敵(frog・queen)。間合い(keepDistance)まで近づいて止まり、そこから自機へ撃ち続ける。
/// 女王も今はこのクラスで、違いは数値(動かない・硬い・発射間隔が短い)とクリア条件(isGoal)だけ。
/// 女王に固有の動き(攻撃パターンなど)を作るときは、このクラスを継承したQueenEnemyを足し、
/// UpdateBehaviorを上書きする(撃つ処理はTryShootを使い回せる)
/// </summary>
class ShooterEnemy : public Enemy {
public:
	BehaviorResult UpdateBehavior(const EnemyTypeParams& params, const Context& context) override;

protected:
	// 初回の発射までの待ちを決める。出現と同時に撃たれると反応できないので、初回も同じ間隔だけ待たせる
	void OnSpawn(const EnemyTypeParams& params, const Context& context) override;

	/// <summary>
	/// クールタイムを消化し、撃てる状況なら自機へ1発撃つ。移動の後に呼ぶこと(撃つ瞬間の位置から弾を出すため)
	/// </summary>
	/// <returns>撃ったか</returns>
	bool TryShoot(const EnemyTypeParams& params, const Context& context);

private:
	// 次のクールタイムを引く。個体差を付けて斉射を防ぐ(出現時の初回待ちにも使う)
	static float NextShootCooldown(const EnemyTypeParams& params, float intervalRandom);

	// 次に撃つまでの残り時間[秒]。個体ごとに持たせるのは、
	// 共通タイマーだと同時に出た敵が必ず同時に撃ち、避けようのない斉射になるため。
	// 撃つ敵にしか要らない状態なので、基底(Enemy)ではなくここに置く
	float shootCooldown_ = 0.0f;
};
