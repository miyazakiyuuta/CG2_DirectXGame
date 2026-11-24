#pragma once
#include "math/Vector2.h"
#include "ui/IHudElement.h"
#include "math/Vector3.h"

#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class Sprite;

// 照準(レティクル)。ワールド上の狙点(Player::GetAimPoint)をスクリーンへ投影し、
// 「弾がどこへ飛ぶのか」を画面上に出す。
// チャージの溜まり具合と、ロック中の敵を囲む枠もここで描く(照準まわりの表示を1か所に集める)。
//
// なぜ専用クラスにするか: 「3D座標→スクリーン座標」の変換と十字の組み立ては
// HUD(HP・スコア)の固定レイアウトとは別物で、シーンに置くと描画準備が肥大するため。
// application/ui/ に置くのはNumberSpriteと同じ理由(エンジンの汎用機能ではなくゲーム固有のUI)。
//
// 座標の基準はシーンRT(WinApp::kClientWidth×kClientHeight)のピクセル座標。
// エディタのSceneビューはこのRTを拡縮表示しているだけなので、RT内のピクセルで置けば
// 全構成(Debugのビュー内表示 / Development・Releaseの全画面)で同じ位置に出る。
// Input::GetSceneImagePos()を使わないのは、あれがUSE_IMGUI限定かつ
// 「ImGuiのDrawListへ直接描くオーバーレイ」用の値だから(スプライトはRTの中に描かれる)。
class Reticle : public IHudElement {
public:
	// 十字とロック枠を構成するスプライトを作る
	void Initialize();

	/// <summary>
	/// 狙点を投影して各パーツの位置と色を決める。Drawの前に毎フレーム呼ぶ。
	/// カメラ背面に回った狙点(レール終端でのクランプ等)は非表示にする
	/// </summary>
	/// <param name="aimPoint">狙点のワールド座標(弾と共有する点)</param>
	/// <param name="camera">描画に使っているカメラ(デバッグカメラON中はそちら)</param>
	/// <param name="deltaTime">満タン時の脈打ちとロック枠の吸い付く動きを進める時間[秒]。
	/// ヒットストップで止めないよう実dtを渡す(HUDのフェードインと同じ)</param>
	void Update(const Vector3& aimPoint, const Camera& camera, float deltaTime);

	// 呼び出し側が先にSpriteCommon::CommonDrawSetting()を済ませていること(HUDと同じ経路)
	void Draw() override;

	/// <summary>
	/// 狙点が敵に重なっているか。trueで色が変わる。
	/// 判定そのものは敵側(EnemySpawner)が持ち、ここは結果を受け取って見せるだけ。
	/// ロックオンとは別物(通常弾の狙点が敵の中にあるか)なので、名前も分けてある
	/// </summary>
	void SetAimingAtEnemy(bool aiming) { aimingAtEnemy_ = aiming; }

	/// <summary>
	/// チャージの溜まり具合(0〜1)。溜まるほど十字が中心へ寄ってチャージ弾の色(黄緑)へ近づき、
	/// 満タン(1)で脈打って「離せば撃てる」ことを知らせる
	/// </summary>
	void SetCharge(float ratio) { chargeRatio_ = ratio; }

	/// <summary>
	/// ロック中の敵を囲む枠を出す。Updateより前に毎フレーム呼ぶ。
	/// targetIdは「前のフレームと同じ相手か」の見分けにだけ使い、相手が変わったら吸い付く動きを最初から再生する
	/// </summary>
	/// <param name="targetId">ロック中の敵の番号(0ならロックなし=枠を出さない)</param>
	/// <param name="center">敵の中心(ワールド座標)</param>
	/// <param name="radius">敵の判定半径[m]。画面上の枠の大きさに使う(遠い敵ほど小さく囲む)</param>
	void SetLockTarget(uint32_t targetId, const Vector3& center, float radius);
	// ロックなし(枠を出さない)
	void ClearLockTarget() { SetLockTarget(0, {}, 0.0f); }

	/// <summary>
	/// 照準を引き寄せる先(ロックオンの吸い付き)。Updateより前に毎フレーム呼ぶ。
	/// 画面上の照準の位置だけを動かす見た目の演出で、弾の向き(狙点)は変えない
	/// (チャージ中は通常弾が出ないので、照準と弾の向きがずれても困らない)
	/// </summary>
	/// <param name="target">引き寄せる先(ワールド座標)。敵の中心</param>
	/// <param name="amount">引き寄せの強さ。0で本来の位置、1で先の真上にぴったり重なる</param>
	void SetAssistTarget(const Vector3& target, float amount);
	// 引き寄せなし(照準は本来の位置へ滑らかに戻っていく)
	void ClearAssistTarget() { SetAssistTarget({}, 0.0f); }

	/// <summary>
	/// 全体の透明度の倍率(0〜1)。HUDのフェードイン用。
	/// 色そのものはUpdateで状態(敵に重なっているか・チャージ)から決まるので、外からは濃さだけを渡す。Updateより前に呼ぶこと
	/// </summary>
	void SetAlpha(float alpha) override { alpha_ = alpha; }

	Reticle();
	~Reticle() override;

private:
	// 十字の各パーツ(4本+中心点)の位置と大きさを両方の列へ同じ規則で書き込む。
	// gapは中心から各本の内側の端まで[px](1280基準)、sizeScaleは脈打つ演出の倍率、
	// expandは縁取り側だけ大きくするための上乗せ量[px]
	void LayoutParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
		float uiScale, float gap, float sizeScale, float expand) const;

	// ロック枠(四隅のカギ型)の位置と大きさを書き込む。halfSizeは枠の中心から角までの距離[px](実解像度)
	void LayoutLockParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
		float halfSize, float uiScale, float expand) const;

	// 十字の4本+中心点。専用の画像素材を持たず、白1枚を色付きで並べて組む
	// (素材を増やさずに済み、色替えだけで状態の表示も作れる)。
	// 同じ形を「黒で一回り大きく」→「本体の色で」の2度描きにしているのは、
	// 明るい空や雪原の上でも線が消えないようにするため
	// (紹介動画・週報の動画で照準が映っていることが採点対象になる)
	std::vector<std::unique_ptr<Sprite>> outlines_;
	std::vector<std::unique_ptr<Sprite>> fills_;
	// ロック枠。四隅×(横棒+縦棒)の8本で、十字と同じく縁取り→本体の2度描き
	std::vector<std::unique_ptr<Sprite>> lockOutlines_;
	std::vector<std::unique_ptr<Sprite>> lockFills_;

	// 今フレーム描くか(カメラ背面なら描かない)
	bool visible_ = false;
	bool aimingAtEnemy_ = false;
	// 透明度の倍率(SetAlphaで外から渡される)
	float alpha_ = 1.0f;

	// チャージの溜まり具合(0〜1、SetChargeで外から渡される)と、満タン時の脈打ちに使う経過時間[秒]
	float chargeRatio_ = 0.0f;
	float pulseTime_ = 0.0f;

	// 照準を引き寄せる先(ワールド座標)と強さ(0〜1)。SetAssistTargetで外から渡される
	Vector3 assistTarget_{};
	float assistAmount_ = 0.0f;
	// 画面上で照準を本来の位置からずらしている量[px]。目標のずれへ毎フレーム少しずつ近づけるので、
	// 吸い付く先が入れ替わったときや、撃って本来の位置へ戻るときも瞬間移動せず滑らかに動く
	Vector2 assistOffset_{};

	// ロック中の敵(SetLockTargetで外から渡される)。番号0はロックなし
	uint32_t lockTargetId_ = 0;
	Vector3 lockCenter_{};
	float lockRadius_ = 0.0f;
	// 今の相手をロックしてからの経過時間[秒](吸い付く動きの進み具合)
	float lockTime_ = 0.0f;
	// 今フレーム枠を描くか(ロックなし・カメラ背面なら描かない)
	bool lockVisible_ = false;
};
