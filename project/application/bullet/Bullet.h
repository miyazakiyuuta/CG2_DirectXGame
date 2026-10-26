#pragma once
#include "math/Vector3.h"

#include <cstdint>

// 弾1発分の「状態」。見た目(Object3d)は持たず、BulletManagerがindex対応で別に所有する。
// なぜ分けるか: 弾はプールで使い回すため実体の生存期間とゲーム状態の生存期間が一致しない
// (activeがfalseでもObject3dは生き続ける)。混ぜると「消えた弾のObject3dをdeleteすべきか」を
// 毎回考えることになる。StageDataとStage::objects_の関係と同じ分け方。
// 速度・寿命・半径の既定値はBulletManagerが持つ(ImGuiで全弾一括調整するため)。
struct Bullet {
	Vector3 position{};
	// 進行速度[m/s]。発射時に「速さ×方向」で確定する。
	// 追尾する弾(targetIdあり)は毎フレーム向きだけを変え、速さは変えない
	Vector3 velocity{};
	// 残り寿命[秒]。0以下になったら消える(画面外へ飛び続ける弾の掃除)
	float lifeTime = 0.0f;
	// 追尾する相手の番号。0なら追尾しない(まっすぐ飛ぶ)。
	// 番号から位置を引くのはBulletManagerに渡された窓口で、弾は相手が何者か(敵かどうか)を知らない
	uint32_t targetId = 0;
	// 尾のパーティクルの端数。「毎秒の粒数×dt」が1粒に満たない分を次のフレームへ持ち越す
	float trailCarry = 0.0f;
	// 使用中か。falseの弾はプールの空きスロットを意味する
	bool active = false;
};
