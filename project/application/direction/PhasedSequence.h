#pragma once
#include "direction/ISequence.h"

/// <summary>
/// 「段階」で進む演出の骨組み。ゲームオーバー演出とクリア演出が継承する(ConvolutionEffectと同じ中間クラス)。
///
/// 2つの演出は、毎フレームの更新の「順番」がまったく同じだった:
///   始まる前・終わった後は何もしない → 時計を進める → 段階を進める → 時間倍率を決める
///   → 粒の時間倍率をそろえる → 演出の物体を動かす → ポストエフェクトを合わせる
/// この順番をUpdate(Template Method)としてここに1度だけ書き、派生クラスには各段の中身だけを書かせる。
/// Template Method: 基底クラスが処理の骨組み(順番)を持ち、違う部分だけを仮想関数として派生クラスに任せる型。
/// Updateにfinalを付けるのは、派生クラスが順番そのものを書き換えられないようにするため
/// (順番を崩すと「段階が変わったフレームの時間倍率」などが1フレームずれる)。
///
/// 段階そのもの(enum Phase)は演出ごとに中身が違うので、ここでは持たない。
/// ここが持つのは、どの演出でも同じ意味を持つ時計2本と時間倍率だけ
/// </summary>
class PhasedSequence : public ISequence {
public:
	void Update(float deltaTime, const Camera& camera) final;

	float GetTimeScale() const override { return timeScale_; }

protected:
	// --- 派生クラスが中身を書く段(Updateが上から順に呼ぶ) ---

	// 段階を進める対象か。始まる前と終わった後はfalseを返す(何も触らない)
	virtual bool IsRunning() const = 0;
	// 今の段階の長さを過ぎていれば、次の段階へ進める
	virtual void AdvancePhase() = 0;
	// 今の段階での時間倍率(ヒットストップ=0 / スロー / 等速=1)
	virtual float ComputeTimeScale() const = 0;
	// ヒットストップ中か。この間だけ粒は等速のまま流す(止まった画面の中で火花だけが散る見せ方)
	virtual bool IsHitStop() const = 0;
	/// <summary>
	/// 演出の物体(墜落する機体・女王の亡骸)と粒を、今の段階に合わせて動かす
	/// </summary>
	/// <param name="deltaTime">実時間[秒]。ゲーム時間はdeltaTime × GetTimeScale()</param>
	virtual void UpdateContents(float deltaTime, const Camera& camera) = 0;
	// ポストエフェクトを今の段階に合わせる。終わったフレームにも1度だけ呼ばれる
	virtual void ApplyPostEffects(const Camera& camera) = 0;

	// --- 派生クラスが使う時計 ---

	// 演出を始める・止めるときに呼ぶ。時計を0へ、時間倍率を等速へ戻す
	void ResetClocks();
	// 段階を切り替えたときに呼ぶ(段階内の経過時間を0へ戻す)
	void RestartPhaseTimer() { phaseTimer_ = 0.0f; }
	// 段階を外から切り替えた直後(Start)に、時間倍率をその段階の値へ合わせる
	void RefreshTimeScale() { timeScale_ = ComputeTimeScale(); }

	// 今の段階に入ってからの経過時間[秒](実時間)
	float GetPhaseTimer() const { return phaseTimer_; }
	// 演出が始まってからの経過時間[秒](実時間)。段階の区切りに関係なく数えたいもの(被弾の赤・白い縁など)に使う
	float GetElapsed() const { return elapsed_; }

private:
	float phaseTimer_ = 0.0f;
	float elapsed_ = 0.0f;
	// 今フレームのゲーム時間の倍率
	float timeScale_ = 1.0f;
};
