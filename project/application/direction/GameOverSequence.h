#pragma once
#include "math/Vector3.h"
#include "effect/ParticleConfig.h"

#include <memory>

class Camera;
class EffectManager;
class Object3d;
class ParticleEmitter;
struct SoundData;

// ゲームオーバーの演出。致命的な被弾 → 制御不能 → 遠ざかる → 爆発 → 暗転 の5段階を、経過時間で順に進める
// (段階=Phaseのステートマシン。「今どの段階か」と「次へ進む条件(経過時間)」だけで進行が決まる)。
//
// なぜ専用クラスにするか: IntroCameraと同じく「経過時間 → 機体・パーティクル・ポストエフェクトの状態」という
// 閉じた計算で、調整値もここでしか使わないため。ゲーム進行を止める判断(レール・操作・HUD)と、
// 画面の揺れ・結果画面への遷移はシーンが行い、このクラスはゲームの状態を知らない。
//
// 時間は2本の時計で進める。
// - 実時間(Updateの引数): 段階のタイマーとポストエフェクト。スローモーションの間も演出の長さは変わらない
// - ゲーム時間(実時間 × GetTimeScale()): 墜落する機体の動き。シーンは同じ倍率を敵・弾にも掛ける
// ヒットストップは「倍率0の区間」、スローモーションは「倍率0.25の区間」で、同じ仕組みの強さ違いになる。
//
// 墜落する機体は自機(Player)とは別のObject3dで描く。Playerは位置を「レール上の距離+断面内のずれ」で
// 表す前提で作られていて、レールを離れて自由に飛ぶ動きを入れるとその前提が崩れるため
// (見た目はPlayer::ModelPathでそろえる。TitleSceneの自機と同じ作り)
class GameOverSequence {
public:
	enum class Phase {
		None,     // 再生していない
		Impact,   // 致命的な被弾。時間を止め(ヒットストップ)、被弾箇所から火花を散らす
		Spin,     // 制御不能。スローモーションで回転し、被弾箇所から煙と火花が出続ける
		Drift,    // 遠ざかる。時間の速さが戻り、回転しながら前方へ流される。カメラは止まった位置から見送る
		Explode,  // 爆発。機体を消し、爆発の粒を一斉に出す
		FadeOut,  // 暗転。爆発の余韻を残したまま、残りの色を抜き切って真っ黒にする
		Finished, // 真っ黒になった。シーンが結果画面への遷移を頼む
	};

	/// <summary>
	/// 演出の時間と動きの量。ImGui(Debug構成)から調整できるようメンバに持つ。
	/// 段階の長さは実時間、機体の動きはゲーム時間(スロー中は遅く進む)で数える
	/// </summary>
	struct Params {
		// --- 段階の長さ[秒](実時間) ---
		float impactDuration = 0.25f;  // 致命的な被弾(ヒットストップ)
		float spinDuration = 2.0f;     // 制御不能(スローモーション)
		float driftDuration = 2.0f;    // 遠ざかる
		float explodeDuration = 0.5f;  // 爆発
		float fadeDuration = 2.0f;     // 暗転

		// --- 時間の速さ ---
		float slowTimeScale = 0.25f;   // 制御不能の間の時間倍率(0.25=4分の1の速さ)
		float recoverDuration = 0.5f;  // 遠ざかり始めてから等速に戻るまで[秒](急に戻すと動きが跳ねて見える)

		// --- 機体の動き(ゲーム時間) ---
		float startSpeed = 3.0f;        // 被弾した瞬間に機体が持っている前向きの速さ[m/s]
		// 遠ざかる間、前方へ流される加速度[m/s^2]。爆発がカメラから約27m先になる強さ
		// (強すぎると遠くで爆発して小さく見える。段階の長さを変えたら合わせて見直す)
		float driftAcceleration = 5.0f;
		float fallAcceleration = 1.5f;  // 制御を失ってから下へ落ちていく加速度[m/s^2](爆発までに約4m沈む)
		float rollSpeed = 6.0f;         // 機首の軸まわりの回転の速さ[rad/s](等速で1秒に約1回転)
		float tumbleSpeed = 0.35f;      // 機首が下がっていく速さ[rad/s](爆発までに約50度下がる)
		// カメラが機体をどれだけ目で追うか(0=レールの向きのまま / 1=機体を画面の中心に捉え続ける)。
		// 位置は止まったまま、向きだけをこの割合で機体へ向ける
		float cameraFollow = 0.6f;

		// --- 画面の揺れ(最大振幅[m]。揺らすのはシーン) ---
		float impactShake = 0.9f;    // 致命的な被弾。通常の被弾(0.5)より強くする
		float explosionShake = 0.6f; // 爆発。機体はもう遠くにいるので被弾より弱め

		// --- ポストエフェクト ---
		float impactVignette = 0.9f;  // 被弾の瞬間の赤の濃さ(Vignetteのintensity)
		// 赤が消え切るまでの時間[秒](演出が始まってからの実時間)。
		// 普段の被弾演出(GamePlaySceneのdamageEffectDuration_)と同じ長さにして、赤が出る長さをそろえる
		float flashDuration = 0.45f;
		// 赤が消えた後、爆発まで保つ黒い縁の濃さ。
		// 赤のままにしておくと、爆発の橙色が縁の赤とにじんでしまうので、黒へ移しておく
		float darkVignette = 0.5f;
		float impactBlur = 0.8f;      // 被弾の瞬間のRadialBlurの強さ
		float impactBlurDuration = 0.35f; // その長さ[秒]
		float spinBlur = 0.25f;       // 制御不能の間、弱く掛け続けるRadialBlurの強さ
		float explosionBlur = 1.0f;   // 爆発の瞬間のRadialBlurの強さ
		float explosionBlurDuration = 0.3f;
		// 赤が消えた後、爆発までにゆっくり抜いておく色の量(Monochromeのintensity。0=元の色 / 1=白黒)。
		// 爆発までに白黒にし切らないのは、パーティクルもポストエフェクトより前に描かれるので、
		// 爆発の炎や火花まで灰色になってしまうから。爆発には色を残す
		float drainMonochrome = 0.5f;
		float desaturateRatio = 0.6f; // 暗転の前半この割合までに、抜けかけの色を白黒にし切る(残りは暗くなるだけ)

		// --- パーティクル ---
		float smokeRate = 60.0f;      // 被弾箇所から出る煙[粒/秒]
		float sparkRate = 30.0f;      // 被弾箇所から出る火花[粒/秒]
		int impactSparkCount = 40;    // 被弾の瞬間に散らす火花の数
		int explosionCount = 160;     // 爆発の炎の数
		int explosionSparkCount = 80; // 爆発の火花の数
		int explosionSmokeCount = 40; // 爆発の跡に残る煙の数(暗転中の余韻)
	};

	// 定義は.cpp側(unique_ptrの破棄に完全型が要るため)
	GameOverSequence();
	~GameOverSequence();

	/// <summary>
	/// 墜落する機体のモデル・パーティクル・爆発音を用意する。シーンのInitializeで1回呼ぶ
	/// </summary>
	/// <param name="effectManager">ポストエフェクトを名前で引く先(シーンが持っているもの)</param>
	void Initialize(EffectManager* effectManager);

	/// <summary>
	/// 演出を始める。墜落する機体を自機と同じ位置・姿勢に置き、被弾の瞬間の火花とRadialBlurを出す
	/// </summary>
	/// <param name="position">自機のワールド位置</param>
	/// <param name="rotate">自機の回転(Player::GetRotate)</param>
	/// <param name="hitSource">最後に当たった相手の位置。nullptrなら機体の決まった点(右翼の付け根)から煙を出す</param>
	void Start(const Vector3& position, const Vector3& rotate, const Vector3* hitSource);

	// 再生していない状態へ戻す(ゲーム状態のリセットとシーンの終了で使う)。
	// 時間倍率・粒・ポストエフェクトは全シーン共有なので、ここで必ず元に戻す
	void Stop();

	// 制御不能・遠ざかる の途中なら、その場で爆発へ進める(スキップ入力)
	void Skip();

	/// <summary>
	/// 段階を進め、機体・パーティクル・ポストエフェクトを今の段階に合わせる
	/// </summary>
	/// <param name="deltaTime">実時間[秒](ヒットストップやスローの影響を受けない時間)</param>
	/// <param name="camera">描画に使っているカメラ(RadialBlurの中心を機体の画面位置に合わせるため)</param>
	void Update(float deltaTime, const Camera& camera);

	// 墜落する機体を描く(爆発した後は何も描かない)
	void Draw();

	void SetCamera(Camera* camera);

	/// <summary>
	/// カメラの回転を求める。位置はレール上で止まったまま、向きだけを機体(爆発後は爆発した点)へ向ける
	/// </summary>
	/// <param name="cameraPosition">カメラの位置(レールカメラの止まった位置)</param>
	/// <param name="railForward">レールの接線(正規化済み)。機体を追う前のカメラの向き</param>
	/// <param name="outRotate">カメラの回転(オイラー角。レールカメラと同じ回転規約)</param>
	void ComputeCameraRotate(const Vector3& cameraPosition, const Vector3& railForward, Vector3& outRotate) const;

	Phase GetPhase() const { return phase_; }
	// 演出中か(Finishedも含む)。シーンはこの間、レールと操作を止め、自機の代わりに墜落する機体を描く
	bool IsActive() const { return phase_ != Phase::None; }
	// 真っ黒になったか。シーンは結果画面への遷移を頼む
	bool IsFinished() const { return phase_ == Phase::Finished; }
	// ゲーム時間の倍率。シーンは敵・弾に渡すdeltaTimeにこれを掛ける
	float GetTimeScale() const { return timeScale_; }
	const Params& GetParams() const { return params_; }

	// "Game Over"ウィンドウに進み具合と調整値を出す(Debug構成のみ)
	void DrawImGui();

private:
	// 段階を切り替え、段階内の経過時間を0へ戻す。入った瞬間の出来事(爆発など)もここで起こす
	void EnterPhase(Phase phase);

	// 今の段階での時間倍率
	float ComputeTimeScale() const;

	// 暗転の進み具合(暗転前=0 〜 真っ黒=1)
	float GetFadeProgress() const;

	// 機体を描くか(爆発する前の段階か)
	bool IsWreckVisible() const;

	/// <summary>
	/// 被弾箇所を機体のローカル座標で求める。当たった相手の方向を機体の軸で測り直し、機体の表面あたりの点にする
	/// </summary>
	Vector3 ComputeLocalHitPoint(const Vector3& position, const Vector3& rotate, const Vector3* hitSource) const;

	// 機体を動かし(ゲーム時間)、姿勢と被弾箇所を決める
	void UpdateWreck(float gameDeltaTime);
	// 今の位置・回転をObject3dへ渡し、被弾箇所のワールド座標を求める
	void ApplyWreckPose();

	// 煙と火花のエミッタを被弾箇所へ動かし、出すかどうかを段階で決める
	void UpdateEmitters();

	// 爆発の瞬間。炎・火花・煙を一斉に出し、爆発音を鳴らし、RadialBlurを一瞬強める
	void Explode();

	// Vignette・RadialBlur・Monochromeを今の段階に合わせる
	void ApplyPostEffects(const Camera& camera);

	EffectManager* effectManager_ = nullptr;
	Params params_;

	Phase phase_ = Phase::None;
	// 今の段階に入ってからの経過時間[秒](実時間)
	float phaseTimer_ = 0.0f;
	// 演出が始まってからの経過時間[秒](実時間)。被弾の赤を、段階の区切りに関係なく
	// 普段の被弾と同じ長さで消すために、段階のタイマーとは別に数える
	float elapsed_ = 0.0f;
	// 今フレームのゲーム時間の倍率
	float timeScale_ = 1.0f;

	// --- 墜落する機体 ---
	std::unique_ptr<Object3d> wreck_;
	Vector3 position_ = { 0.0f, 0.0f, 0.0f }; // 爆発した後は爆発した点(カメラとRadialBlurが見る先)
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	Vector3 startRotate_ = { 0.0f, 0.0f, 0.0f }; // 被弾した瞬間の姿勢(自機と同じ)
	Vector3 forward_ = { 0.0f, 0.0f, 1.0f };     // 被弾した瞬間の機首の向き(前方へ流される向き)
	float roll_ = 0.0f;   // 機首の軸まわりの回転[rad](機体のローカル)
	float tumble_ = 0.0f; // 機首が下がる回転[rad](機体のローカル)
	// 被弾箇所。ローカル座標で持って毎フレーム機体の回転を掛けるので、機体と一緒に回る
	Vector3 localHitPoint_ = { 0.0f, 0.0f, 0.0f };
	Vector3 hitPoint_ = { 0.0f, 0.0f, 0.0f }; // そのワールド座標(煙と火花の出所)
	// 今カメラが機体をどれだけ追っているか(0 〜 params_.cameraFollow)。遠ざかる間に少しずつ上げる
	float cameraFollow_ = 0.0f;

	// --- パーティクル ---
	// 被弾箇所から出続ける煙と火花。エミッタはParticleManagerが毎フレーム(時間倍率つきで)更新する
	std::unique_ptr<ParticleEmitter> smokeEmitter_;
	std::unique_ptr<ParticleEmitter> sparkEmitter_;
	// 爆発の炎の見た目(爆発の瞬間に一度だけ出すのでエミッタは使わない)
	ParticleConfig explosionConfig_;

	// 爆発音。SoundManagerのキャッシュと共有し、再生中のPCMを有効に保つ
	std::shared_ptr<const SoundData> explosionSound_;
};
