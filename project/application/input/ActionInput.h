#pragma once

// ゲーム内アクションと物理入力(KB/マウス/XInput)の対応付けを一手に引き受ける層。
// 利用側(Player等)は DIK_ や XINPUT_GAMEPAD_ を直接参照せず、アクションだけを読む。
// なぜ: 最終的にパッド主流の方針のため、キー割当の変更をこのファイルに閉じ込める
// (後付けだと利用側の全キー参照を書き換えることになる)。
// 「Shoot/Boost」というゲーム固有の語彙を含むため、エンジン層(engine/io)ではなく
// application/ に置く(engine/io/Input はデバイス層の汎用コードのまま触らない)。
class ActionInput {
public:
	// デジタル入力のアクション(押した/押している で読むもの)
	enum class Action {
		Shoot, // 射撃(⑪で使用予定。現状は定義のみ)
		Boost, // ブースト(課題Bで使用予定。現状は定義のみ)
	};

	/// <summary>
	/// 左右の移動軸を取得(-1.0f〜+1.0f)。KB: A/D・←/→、パッド: 左スティックX。
	/// 併用時は合算してクランプする
	/// </summary>
	float GetMoveX() const;

	/// <summary>
	/// 上下の移動軸を取得(-1.0f〜+1.0f、上が+)。KB: W/S・↑/↓、パッド: 左スティックY
	/// </summary>
	float GetMoveY() const;

	/// <summary>
	/// アクションが押されているか
	/// </summary>
	bool IsPress(Action action) const;

	/// <summary>
	/// アクションが今押されたか
	/// </summary>
	bool IsTrigger(Action action) const;
};
