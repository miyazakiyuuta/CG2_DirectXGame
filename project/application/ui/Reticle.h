#pragma once
#include "math/Vector2.h"

#include <memory>
#include <vector>

class Camera;
class Sprite;
struct Vector3;

// 照準(レティクル)。ワールド上の狙点(Player::GetAimPoint)をスクリーンへ投影し、
// 「弾がどこへ飛ぶのか」を画面上に出す。
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
class Reticle {
public:
	// 十字を構成するスプライトを作る
	void Initialize();

	/// <summary>
	/// 狙点を投影して各パーツの位置と色を決める。Drawの前に毎フレーム呼ぶ。
	/// カメラ背面に回った狙点(レール終端でのクランプ等)は非表示にする
	/// </summary>
	/// <param name="aimPoint">狙点のワールド座標(弾と共有する点)</param>
	/// <param name="camera">描画に使っているカメラ(デバッグカメラON中はそちら)</param>
	void Update(const Vector3& aimPoint, const Camera& camera);

	// 呼び出し側が先にSpriteCommon::CommonDrawSetting()を済ませていること(HUDと同じ経路)
	void Draw();

	/// <summary>
	/// 狙点が敵に重なっているか。trueで色が変わる。
	/// 判定そのものは敵側(EnemySpawner)が持ち、ここは結果を受け取って見せるだけ
	/// </summary>
	void SetLockedOn(bool lockedOn) { lockedOn_ = lockedOn; }

	Reticle();
	~Reticle();

private:
	// 各パーツ(十字4本+中心点)の位置と大きさを両方の列へ同じ規則で書き込む。
	// expandは縁取り側だけ大きくするための上乗せ量[px]
	void LayoutParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
		float uiScale, float expand) const;

	// 十字の4本+中心点。専用の画像素材を持たず、白1枚を色付きで並べて組む
	// (素材を増やさずに済み、色替えだけでロックオン表示も作れる)。
	// 同じ形を「黒で一回り大きく」→「本体の色で」の2度描きにしているのは、
	// 明るい空や雪原の上でも線が消えないようにするため
	// (紹介動画・週報の動画で照準が映っていることが採点対象になる)
	std::vector<std::unique_ptr<Sprite>> outlines_;
	std::vector<std::unique_ptr<Sprite>> fills_;

	// 今フレーム描くか(カメラ背面なら描かない)
	bool visible_ = false;
	bool lockedOn_ = false;
};
