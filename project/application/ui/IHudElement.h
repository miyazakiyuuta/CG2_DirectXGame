#pragma once

/// <summary>
/// プレイ中に画面へ重ねるHUD部品(照準・進行ゲージ・区間の結果表示)のインターフェース。
/// シーンはHUD全体を1つの濃さでフェードさせ、まとめて描くので、その2つだけを共通の操作にする
/// (GamePlaySceneはこの型のポインタの一覧を回し、部品の種類ごとに同じ呼び出しを並べない)。
///
/// 基底に置かないもの: Update。部品ごとに要る情報がまったく違う(照準=狙点とカメラ / 進行ゲージ=区間の集計と進行度 /
/// 区間の結果表示=経過時間だけ)ので、無理に1つの引数へまとめると、どの部品にも要らない引数を渡すことになる。
/// Updateは具象の型のまま、シーンが部品ごとに必要な情報を渡して呼ぶ
/// </summary>
class IHudElement {
public:
	virtual ~IHudElement() = default;

	// 全体の透明度の倍率(0〜1)。HUDのフェードに合わせる。部品のUpdateより前に呼ぶこと
	virtual void SetAlpha(float alpha) = 0;

	// 呼び出し側が先にSpriteCommon::CommonDrawSetting()を済ませていること。
	// 今は見せる物が無い部品(表示中でない結果表示・カメラの背面にある狙点)は何も描かない
	virtual void Draw() = 0;
};
