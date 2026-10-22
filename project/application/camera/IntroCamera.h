#pragma once
#include "math/Vector3.h"

// ゲーム開始のカメラ演出(イントロ)。自機の前方斜め上から回り込み、レールカメラの定位置に着いたら終わる。
//
// なぜ専用クラスにするか: 「経過時間とレール上の自機 → カメラの位置と回転」という閉じた計算で、
// 調整値もここでしか使わないため。ゲーム進行を止める判断(レール・操作・HUD)はシーンが
// IsPlaying()を見て行い、このクラスはゲームの状態を知らない(Reticleが表示だけを担当するのと同じ分担)。
//
// 位置は自機を中心とした球の上(方位・仰角・距離)で補間し、回転は毎フレーム「自機を見る向き」から求める。
// 角度を直接補間しないのは、前から後ろへ回り込むとyawが180度変わり、atan2の±πの境目で逆回りしうるため。
// 終点(方位0・仰角0・距離backDistance)はレールカメラの位置・回転と一致するので、終わった瞬間に画面が飛ばない
class IntroCamera {
public:
	/// <summary>
	/// 演出の時間と軌道。ImGui(Debug構成)から調整できるようメンバに持つ。
	/// 位置は速度を積み上げず「経過時間 ÷ 長さ」にイージングを掛けて毎フレーム求める(TitleSceneと同じ作り)
	/// </summary>
	struct Params {
		// 冒頭で止まっている時間[秒]。シーン開始直後はBlindTransitionが開いている途中(約1.1秒)なので、
		// 帯が開き始めてから動き出すように少し溜める
		float holdDuration = 0.5f;
		float moveDuration = 2.5f;    // 回り込みにかける時間[秒]
		// 開始の方位[rad]。0=真後ろ / π=真正面。正で自機の右側から回り込む(約150度=右前方)
		float startAzimuth = 2.6f;
		// 開始の仰角[rad](約55度)。真上はyawが決まらないので、ComputePoseで上限を掛ける
		float startElevation = 0.95f;
		float startDistance = 20.0f;  // 開始時の自機からの距離[m]
	};

	// 再生を始める(経過時間を0に戻す)
	void Start();

	// 再生を終わらせる(スキップ入力とゲーム状態のリセットで使う)。
	// 終点はレールカメラと同じなので、止めるだけでカメラは定位置へ移る
	void Stop();

	// 経過時間を進め、回り込み終えたら再生を終える
	void Update(float deltaTime);

	bool IsPlaying() const { return playing_; }

	/// <summary>
	/// 今の経過時間でのカメラの位置と回転を求める。終点ではレールカメラと同じ値になる
	/// </summary>
	/// <param name="target">見つめる点(自機の位置)</param>
	/// <param name="forward">レールの接線(正規化済み)。方位0はこの逆向き=自機の真後ろ</param>
	/// <param name="backDistance">終点での自機からの距離(レールカメラの後方距離)</param>
	/// <param name="outTranslate">カメラの位置</param>
	/// <param name="outRotate">カメラの回転(オイラー角。レールカメラと同じ回転規約)</param>
	void ComputePose(const Vector3& target, const Vector3& forward, float backDistance,
		Vector3& outTranslate, Vector3& outRotate) const;

	// "Intro Camera"ウィンドウに進み具合と調整値を出す(Debug構成のみ)
	void DrawImGui();

private:
	// 回り込みの進行率(0〜1)。溜めの間は0
	float GetProgress() const;

	Params params_;
	// 再生開始からの経過時間[秒]
	float timer_ = 0.0f;
	bool playing_ = false;
};
