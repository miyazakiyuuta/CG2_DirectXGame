#pragma once
#include "direction/PhasedSequence.h"
#include "math/Transform.h"
#include "math/Vector3.h"
#include "effect/ParticleConfig.h"

#include <memory>
#include <string>

class Camera;
class EffectManager;
class Object3d;

// クリアの演出。女王の撃破 → 断末魔 → 大爆発 → 巣の全滅 → 余韻 の5段階を、経過時間で順に進める
// (GameOverSequenceと同じく、段階=Phaseのステートマシン。「今どの段階か」と「次へ進む条件(経過時間)」だけで進行が決まる)。
//
// なぜ専用クラスにするか: GameOverSequenceと同じく「経過時間 → 亡骸・パーティクル・ポストエフェクトの状態」という
// 閉じた計算で、調整値もここでしか使わないため。ゲーム進行を止める判断(レール・操作・HUD)と、
// 画面の揺れ・残りの敵の全滅・結果画面への遷移はシーンが行い、このクラスはゲームの状態を知らない。
// GameOverSequenceとは、毎フレームの更新の順番(時計 → 段階 → 時間倍率 → 物体と粒 → ポストエフェクト)だけを
// 基底のPhasedSequenceで共有する。中身(墜落する機体 / 女王の亡骸)は別物なので、基底には順番と時計しか置かず、
// 段階の進め方・亡骸の動き・画面の効果はここに書く(中身まで基底へ寄せると、片方にしか無い処理が基底に混ざる)。
//
// 時間は2本の時計で進める(GameOverSequenceと同じ)。
// - 実時間(Updateの引数): 段階のタイマー・亡骸の震えと膨らみ・小爆発の間隔・ポストエフェクト。
//   スローの間も演出の長さは変わらない
// - ゲーム時間(実時間 × GetTimeScale()): シーンが自機・敵・弾に掛ける。粒(CPUパーティクル)にも同じ倍率を掛ける
//
// 女王の亡骸は、EnemySpawnerの敵とは別のObject3dで描く。敵は「HPが0になったら即消える」前提で、
// 当たり判定・ロックオン・区間の集計がその前提に乗っているため、女王だけに「死にかけ」の状態を足すと
// そのすべてに分岐が増える。別の物体にすれば、撃破の処理(撃破音・撃破の爆発)は普段の敵と同じまま変わらない
// (墜落する機体をPlayerと別のObject3dにしたのと同じ考え方)。
class ClearSequence : public PhasedSequence {
public:
	enum class Phase {
		None,        // 再生していない
		Impact,      // 撃破の瞬間。時間を止め(長いヒットストップ)、亡骸を白く光らせて火花を散らす
		Convulse,    // 断末魔。スローモーションの中で亡骸が震えながら膨らみ、体のあちこちで小爆発が続く
		Burst,       // 大爆発。亡骸を消し、炎・火花・煙を一斉に出し、画面を白く飛ばす
		Exterminate, // 巣の全滅。残りの敵が色を失って落ちていく(落とすのはEnemySpawner。シーンが段階の変化を見て伝える)
		Hold,        // 余韻。静かな間を置いてから結果画面へ
		Finished,    // 終わり。シーンが結果画面への遷移を頼む
	};

	/// <summary>
	/// 演出の時間と動きの量。ImGui(Debug構成)から調整できるようメンバに持つ。
	/// 段階の長さは実時間で数える
	/// </summary>
	struct Params {
		// --- 段階の長さ[秒](実時間) ---
		float impactDuration = 0.5f;      // 撃破の瞬間(ヒットストップ)。通常の被弾(0.08秒)の約6倍止めて「特別な撃破」だと伝える
		float convulseDuration = 1.5f;    // 断末魔(スロー)
		float burstDuration = 0.3f;       // 大爆発(白い閃光が引き始めるまで)
		float exterminateDuration = 2.0f; // 巣の全滅
		float holdDuration = 1.0f;        // 余韻

		// --- 時間の速さ ---
		float slowTimeScale = 0.3f; // 断末魔の間の時間倍率

		// --- 亡骸の動き ---
		float swell = 0.2f;        // 大爆発の直前に、元の大きさの何割増しまで膨らむか
		float jitterStart = 0.02f; // 震えの振れ幅(判定半径に対する割合)。断末魔の始め
		float jitterEnd = 0.08f;   // 同じく、大爆発の直前。だんだん激しくする
		float whiteFlashDuration = 0.12f; // 撃破の瞬間に亡骸が白く光る長さ[秒]
		float popFlashDuration = 0.05f;   // 小爆発のたびに亡骸が光る長さ[秒]

		// --- 小爆発 ---
		// 間隔[秒](実時間)。断末魔の始めは間をあけ、大爆発へ向けて詰めていく
		float popIntervalStart = 0.35f;
		float popIntervalEnd = 0.06f;
		// 小爆発を置く距離(判定半径に対する割合)。1より少し大きくして体の表面の外側に出す
		// (内側だと亡骸に隠れて見えない。向きもカメラ側へ寄せる)
		float popSpread = 1.05f;

		// --- 画面の揺れ(最大振幅[m]。揺らすのはシーン) ---
		float impactShake = 0.8f; // 撃破の瞬間
		float popShake = 0.15f;   // 小爆発のたび(弱く、細かく)
		float burstShake = 1.2f;  // 大爆発。この演出で一番強い

		// --- ポストエフェクト ---
		float impactFlash = 0.6f;          // 撃破の瞬間の白い縁の濃さ(Vignetteのintensity)
		float impactFlashDuration = 0.4f;  // その白い縁が消えるまで[秒]
		float darkVignette = 0.45f;        // 断末魔の間に濃くしていく黒い縁の濃さ(緊張を高める)
		float burstFlashDuration = 0.7f;   // 大爆発の白い閃光(画面全体)が引くまで[秒]
		float darkReleaseDuration = 1.5f;  // 大爆発の後、黒い縁が引くまで[秒]
		float impactBlur = 0.8f;           // 撃破の瞬間のRadialBlurの強さ
		float impactBlurDuration = 0.4f;
		float convulseBlur = 0.3f;         // 断末魔の間、大爆発へ向けて強めていく常時のRadialBlurの強さ
		float burstBlur = 1.0f;            // 大爆発の瞬間のRadialBlurの強さ
		float burstBlurDuration = 0.5f;

		// --- パーティクル(粒の数) ---
		int impactSparkCount = 60; // 撃破の瞬間に体から飛び散る火花
		int popFireCount = 18;     // 小爆発1回の炎
		int popSparkCount = 10;    // 小爆発1回の火花
		int burstFireCount = 260;  // 大爆発の炎
		int burstSparkCount = 140; // 大爆発の火花
		int burstSmokeCount = 70;  // 大爆発の跡に残る煙(全滅と余韻の間の背景になる)
	};

	// 定義は.cpp側(unique_ptrの破棄に完全型が要るため)
	ClearSequence();
	~ClearSequence();

	/// <summary>
	/// 亡骸の器・パーティクルのプリセットを用意する。シーンのInitializeで1回呼ぶ
	/// </summary>
	/// <param name="effectManager">ポストエフェクトを名前で引く先(シーンが持っているもの)</param>
	void Initialize(EffectManager* effectManager);

	/// <summary>
	/// 演出を始める。女王が倒れた位置・姿勢・大きさに亡骸を置き、撃破の瞬間の火花とRadialBlurを出す
	/// </summary>
	/// <param name="modelPath">亡骸のモデル(女王と同じもの。EnemySpawner::ModelPathOf)</param>
	/// <param name="transform">倒れた瞬間の位置・回転・大きさ(亡骸を置く)</param>
	/// <param name="center">爆発の中心(女王の判定の球の中心)。前後に長い体では原点(重心)が体の奥にあり、
	/// そこで爆発させると体に隠れるので、見えている前面寄りの点を別に受け取る</param>
	/// <param name="radius">女王の判定半径。小爆発を散らす範囲と、爆発の大きさの基準にする</param>
	void Start(const std::string& modelPath, const Transform& transform, const Vector3& center, float radius);

	// 再生していない状態へ戻す(ゲーム状態のリセットとシーンの終了で使う)。
	// 時間倍率・粒・ポストエフェクトは全シーン共有なので、ここで必ず元に戻す
	void Stop() override;

	// 断末魔の途中なら大爆発へ、全滅・余韻の途中なら終わりへ進める(スキップ入力)
	void Skip() override;

	// (Updateは基底のPhasedSequenceが持つ。cameraはRadialBlurの中心と、小爆発をカメラ側へ寄せるのに使う)

	// 亡骸を描く(大爆発の後は何も描かない)
	void Draw() override;

	void SetCamera(Camera* camera) override;

	Phase GetPhase() const { return phase_; }
	// 演出中か(Finishedも含む)。シーンはこの間、レールと操作を止める
	bool IsActive() const override { return phase_ != Phase::None; }
	// 終わったか。シーンは結果画面への遷移を頼む
	bool IsFinished() const { return phase_ == Phase::Finished; }
	// (ゲーム時間の倍率GetTimeScaleは基底のPhasedSequenceが持つ)
	const Params& GetParams() const { return params_; }
	/// <summary>
	/// 小爆発の通算回数。シーンが更新の前後で比べ、増えていたら小さく揺らす
	/// (EnemySpawner::GetExplosionCountと同じ「結果の差分を見る」やり方。演出はシーンの揺れを知らなくてよい)
	/// </summary>
	int GetPopCount() const { return popCount_; }

	// "Clear"ウィンドウに進み具合と調整値を出す(Debug構成のみ)
	void DrawImGui() override;

private:
	// --- PhasedSequenceの骨組み(Update)から呼ばれる段 ---
	bool IsRunning() const override { return phase_ != Phase::None && phase_ != Phase::Finished; }
	// 今の段階の長さを過ぎていれば次の段階へ進める
	void AdvancePhase() override;
	// 今の段階での時間倍率
	float ComputeTimeScale() const override;
	// 撃破の瞬間(ヒットストップ)か
	bool IsHitStop() const override { return phase_ == Phase::Impact; }
	// 亡骸を震わせ・膨らませ・光らせ、断末魔の小爆発を起こす
	void UpdateContents(float deltaTime, const Camera& camera) override;
	// Vignette・RadialBlurを今の段階に合わせる
	void ApplyPostEffects(const Camera& camera) override;

	// 段階を切り替え、段階内の経過時間を0へ戻す。入った瞬間の出来事(大爆発など)もここで起こす
	void EnterPhase(Phase phase);

	// 断末魔の進み具合(断末魔の前=0 〜 大爆発の直前=1。大爆発より後も1)
	float GetConvulseProgress() const;

	// 亡骸を描くか(大爆発の前の段階か)
	bool IsRemainsVisible() const;

	// 亡骸を震わせ、膨らませ、光らせる
	void UpdateRemains(float deltaTime, float gameDeltaTime);

	// 断末魔の間、間隔を詰めながら小爆発を起こす
	void UpdatePops(float deltaTime);

	// 小爆発1回。体の表面のうちカメラ側の1点で、小さな炎と火花を出し、亡骸を一瞬光らせる
	void Pop();

	// 大爆発の瞬間。炎・火花・煙を一斉に出し、RadialBlurを一瞬強める
	void Burst();

	// 大爆発からの経過時間[秒](実時間)。負なら大爆発はまだ。白い閃光と黒い縁が引いていく時計
	float GetSinceBurst() const { return burstStartTime_ < 0.0f ? -1.0f : GetElapsed() - burstStartTime_; }

	// 女王の大きさに合わせて粒の大きさと速さを拡大した設定を返す。
	// 粒の設定は女王の既定の大きさ(判定半径6m)で決めてあるので、モデルの差し替えで大きさが変わっても釣り合う
	ParticleConfig ScaleToRemains(const ParticleConfig& config) const;

	EffectManager* effectManager_ = nullptr;
	Params params_;

	Phase phase_ = Phase::None;
	// 段階のタイマー・演出の経過時間・時間倍率は基底のPhasedSequenceが持つ。
	// 経過時間(GetElapsed)は、撃破の瞬間の白い縁を段階の区切りに関係なく数えるのに使う
	// 大爆発が起きた時刻(演出の経過時間[秒])。負なら大爆発はまだ。
	// 「大爆発からの経過時間」を自前で積み上げず、演出の経過時間との差で求める(時計を1本増やさない)
	float burstStartTime_ = -1.0f;

	// --- 女王の亡骸 ---
	std::unique_ptr<Object3d> remains_;
	std::string remainsModelPath_; // 今の亡骸のモデル(同じなら作り直さない)
	Transform baseTransform_;      // 倒れた瞬間の姿。震えと膨らみはここからのずれとして毎フレーム決め直す
	Vector3 center_ = { 0.0f, 0.0f, 0.0f }; // 爆発の中心(判定の球の中心。小爆発・大爆発・RadialBlurの中心)
	float radius_ = 1.0f;          // 女王の判定半径
	float flashTimer_ = 0.0f;      // 亡骸が光っている残り時間[秒](実時間)
	Vector3 cameraPosition_ = { 0.0f, 0.0f, 0.0f }; // 小爆発をカメラ側へ寄せるため、毎フレーム控える

	// --- 小爆発 ---
	float popTimer_ = 0.0f; // 次の小爆発までの残り時間[秒](実時間)
	int popCount_ = 0;      // 通算回数(シーンが前後の差で揺れを起こす)

	// --- パーティクルの見た目(女王の既定の大きさに合わせた値。ScaleToRemainsで拡大して使う) ---
	ParticleConfig fireConfig_;
	ParticleConfig sparkConfig_;
	ParticleConfig smokeConfig_;
};
