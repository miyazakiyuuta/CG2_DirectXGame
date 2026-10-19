#pragma once
#include "scene/BaseScene.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "effect/ParticleConfig.h"

#include <cstdint>
#include <memory>

class Camera;
class Object3d;
class Sprite;
struct SoundData;

// タイトル画面。オープニング演出(自機の登場 → 岩をよける → 作品名の登場)の後に
// 開始案内・操作説明を出し、以後は「文言を片付ける → 岩をよける → 作品名の登場」をくり返す。
// 開始入力で出発の演出(隕石がカメラへ迫り、自機は右へよける。隕石が視野を埋める瞬間に真っ黒になる)へ進み、
// 真っ黒になったら次のシーンへ切り替える(開く側はBlindTransition)。次のシーンは通常GamePlaySceneで、
// 画面遷移コンテストのデモ中はTitleScene自身(TitleScene.cppのkDepartNextScene)。
// 文言の表示と入力の作法はResultSceneに揃えている(文言は画像、中央アンカー、1280x720基準で拡大)
class TitleScene :public BaseScene {
public:

	// 定義は.cpp側(unique_ptrの破棄に完全型が要るため。ResultSceneと同じ理由)
	TitleScene();

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	// "Title"ウィンドウに演出の進行状況と調整値を出す(Debug構成のみ)
	void DrawImGui() override;

	~TitleScene() override;

private:
	// オープニング演出の段階。シナリオの1段落 = 1フェーズで、上から順に進む。
	// Rise・Holdは最初の1回だけで、以後はRockPass〜TitleOutをくり返す
	enum class Phase {
		Rise,     // 自機が画面の下から中央へ上昇する
		Hold,     // 中央で静止する
		RockPass, // 岩が右前方から迫り、自機が左へよける。岩が画面外へ抜けたら次へ
		Return,   // 自機が中央へ戻る
		TitleIn,  // 作品名が奥から迫り出してくる
		Ready,    // 開始案内・操作説明を出して、しばらく見せる
		TitleOut, // 作品名が上へ抜け、開始案内・操作説明が消える。終わったらRockPassへ戻る
		Depart,   // 開始入力の後。隕石がカメラへ迫り、自機は右へよける。隕石が視野を埋めて真っ黒になったら次のシーンへ
	};

	/// <summary>
	/// 演出の時間と動きの量。ImGui(Debug構成)から調整できるようメンバに持つ。
	/// 位置や角度は速度を積み上げず「フェーズ内の経過時間 ÷ 長さ」にイージングを掛けて毎フレーム求める
	/// (経過時間だけで状態が決まるので、スキップや再生し直しで状態がずれない)
	/// </summary>
	struct OpeningParams {
		float riseDuration = 1.5f;    // 上昇にかける時間[秒]
		float holdDuration = 2.0f;    // 中央に着いてから岩が来るまで[秒]
		float rockSpeed = 20.0f;      // 岩の速さ[m/s]
		float dodgeLeadTime = 2.5f;   // 岩が自機の位置に届く何秒前によけ始めるか
		float dodgeDuration = 2.5f;   // よけ切るまでの時間[秒]
		float dodgeDistance = 3.0f;   // 左へよける量[m]
		float dodgeBank = 0.7f;       // よけるときの傾き[rad](約40度)
		float returnDelay = 0.3f;     // 岩が抜けてから戻り始めるまで[秒]
		float returnDuration = 2.0f;  // 中央へ戻る時間[秒]
		float titleInDuration = 1.5f; // 作品名が迫り出す時間[秒]
		float readyDuration = 7.0f;   // 作品名が出揃ってから片付け始めるまで[秒]
		float titleOutDuration = 1.0f; // 作品名が上へ抜けていく時間[秒]
	};

	/// <summary>
	/// 出発の演出(Depart)の時間と動きの量。時刻はすべて開始入力からの経過時間[秒]。
	/// 隕石が視野を埋める時刻(=画面が真っ黒になる時刻・次のシーンへ切り替える時刻・BGMが消え切る時刻)と、
	/// 隕石が自機を追い越す時刻(=よけるタイミングの基準)は、隕石の大きさ・速さから計算で決まる
	/// </summary>
	struct DepartParams {
		float meteorSpeed = 40.0f;     // 隕石の速さ[m/s]
		float meteorRadius = 3.0f;     // 隕石の半径[m](直径6m。自機の約5倍)
		// 「視野を埋めた」の判定で、隕石を半径の何倍の球とみなすか。
		// 岩はでこぼこで、輪郭が半径の球より内側へ欠ける所があるので、1より小さくして余裕を持たせる
		float silhouetteRatio = 0.85f;
		float dodgeLeadTime = 0.9f;    // 隕石が自機を追い越す何秒前によけ始めるか
		float dodgeDuration = 0.8f;    // 右へよけ切るまでの時間[秒]
		float darkDuration = 0.3f;     // 視野が埋まる何秒前から暗くし始めるか(この間に画面全体を一様に真っ黒へ)
		float shakeDuration = 0.4f;    // 隕石が自機を追い越すときの揺れ(風圧)の長さ[秒]
		float shakeStrength = 0.15f;   // 揺れの最大振幅[m]
		float trailRate = 200.0f;      // 炎の尾の粒を1秒に何個出すか
		float trailSpreadSpeed = 4.0f; // 尾の粒が輪郭から外へ広がる速さ[m/s]
	};

	// Departに入った瞬間の見た目。ここから動かし始めるので、どのフェーズで押されても位置や文言が飛ばない
	struct DepartStart {
		Vector3 shipPosition = { 0.0f, 0.0f, 0.0f };
		float shipRoll = 0.0f;
		// 飛んでいたタイトルの岩の、RockPass内の経過時間(負 = 岩は出ていなかった)
		float rockTime = -1.0f;
		bool titleVisible = false; // 作品名を描いていたか
		bool guideVisible = false; // 開始案内・操作説明を描いていたか
		Vector2 titlePos = { 0.0f, 0.0f };
		Vector2 titleSize = { 0.0f, 0.0f };
		float titleAlpha = 0.0f;
		float startAlpha = 0.0f;
		float controlsAlpha = 0.0f;
	};

	// 開始入力が入ったか(Enter/Space/パッドA/START)。結果画面のリトライと同じ組み合わせにして、
	// 「決定」の操作をゲーム全体で1つに揃える
	bool IsStartPressed() const;

	// フェーズを切り替え、フェーズ内の経過時間を0へ戻す。
	// Readyへ入ったときは、作品名を出し切ったこと(titleShown_)も記録する
	void ChangePhase(Phase phase);

	// 演出を1フレーム進め、自機と岩の状態を決める。各フェーズの終わりで次のフェーズへ進める
	void UpdateOpening(float deltaTime);

	/// <summary>
	/// 岩をstartからaimへ向けて等速で直進させ、位置と回転を決める(RockPassの岩と、出発時に飛んでいたその岩の続きで共有)
	/// </summary>
	/// <param name="time">飛び始めてからの経過時間[秒]</param>
	/// <returns>岩が画面外へ抜け切ったらfalse</returns>
	bool UpdateRock(Object3d& rock, const Vector3& start, const Vector3& aim, float speed, float time);

	// よける動きの進行率(0〜1)。岩が自機に届く時刻から逆算して始まる
	float GetDodgeProgress() const;

	/// <summary>
	/// 出発の演出を始める。今の自機・岩・文言の見た目を記録してからDepartへ切り替える
	/// </summary>
	/// <param name="preview">trueなら演出だけ再生し、真っ黒になってもシーンを切り替えずReadyへ戻る(Debugの調整用)</param>
	void StartDepart(bool preview);

	// Departの1フレーム。自機・隕石を動かし、視野が埋まって真っ黒になったら次のシーンへ切り替える
	void UpdateDepart(float deltaTime);

	/// <summary>
	/// 隕石の位置と回転を決め、炎の尾の粒を出す
	/// </summary>
	/// <param name="time">開始入力からの経過時間[秒]</param>
	/// <param name="emitTrail">尾の粒を出すか(視野が埋まった後は出さない)</param>
	void UpdateMeteor(float time, float deltaTime, bool emitTrail);

	// 隕石が視野を埋める時刻[秒]。画面の4隅を通る視線がすべて隕石(球とみなす)に当たる時刻
	float GetFillTime() const;

	// 隕石が自機を追い越す時刻[秒](隕石の通り道のうち、自機の定位置に最も近い点を通る時刻)
	float GetMeteorPassTime() const;

	// 右へよけた後の自機と隕石の表面との間隔[m]。負ならすり抜けて見える(ImGuiでの確認用)
	float GetMeteorClearance() const;

	// 視野が埋まる直前に、画面全体を一様に暗くする(Vignetteの強さを上げる)
	void UpdateDarkness();

	// 文言(作品名・開始案内・操作説明)の位置・大きさ・透明度を、今のフェーズに合わせて決める
	void UpdateSprites();

	// 自機の真後ろから見るカメラ(ゲーム本編のレールカメラと同じ距離・画角)
	std::unique_ptr<Camera> camera_;

	// 自機と岩。見た目だけの演出用で、当たり判定や入力は持たない
	std::unique_ptr<Object3d> shipObject_;
	std::unique_ptr<Object3d> rockObject_;
	bool rockVisible_ = false;
	// 出発の演出の隕石(岩のモデルを拡大し、赤熱した色にしたもの)
	std::unique_ptr<Object3d> meteorObject_;
	bool meteorVisible_ = false;

	// 自機の位置と傾き(ロール。正で左に傾く)。フェーズごとに決めてshipObject_へ渡す
	Vector3 shipPosition_ = { 0.0f, 0.0f, 0.0f };
	float shipRoll_ = 0.0f;

	// 作品名(title.png)・開始案内(start.png)・操作説明(controls.png)。
	// エンジンにフォント描画が無いため、文言はすべて画像として持つ
	std::unique_ptr<Sprite> titleSprite_;
	std::unique_ptr<Sprite> startSprite_;
	std::unique_ptr<Sprite> controlsSprite_;
	// 作品名の等倍サイズ[px]。迫り出す演出はこれに倍率を掛けて作る
	Vector2 titleSize_ = { 0.0f, 0.0f };
	// 作品名の定位置[px](中心)。上へ抜ける演出はここから動かし、次に迫り出すときはここへ戻す
	Vector2 titlePos_ = { 0.0f, 0.0f };

	Phase phase_ = Phase::Rise;
	// 今のフェーズに入ってからの経過時間[秒]
	float phaseTimer_ = 0.0f;
	// 作品名を一度出し切ったか(Readyへ入ったらtrue)。
	// 出し切る前(最初の演出中)の開始入力はスキップ、出し切った後はどのフェーズでも即ゲーム開始にする。
	// 開始案内は1周に数秒しか出ないので、消えている間に押されても反応させるため
	bool titleShown_ = false;

	// --- 出発の演出 ---
	DepartStart departStart_;
	// 演出だけの再生か(Debugの調整用。真っ黒になってもシーンを切り替えない)
	bool departPreview_ = false;
	// 次のシーンへの切り替えを頼んだか。頼んだ後はVignetteに触らない
	// (ChangeSceneが全ポストエフェクトを切るので、点け直すと次のシーンへ持ち越してしまう)
	bool sceneChangeRequested_ = false;
	// 隕石が自機を追い越すときの揺れの残り時間[秒]
	float shakeTimer_ = 0.0f;
	// 炎の尾の粒を出すためにためている時間[秒]。1粒ぶんの間隔がたまるごとに1粒出す
	// (フレームの長さに関係なく、1秒あたりの粒の数を一定にするため。ParticleEmitterと同じ作り)
	float trailTimer_ = 0.0f;
	// 炎の尾の粒の見た目(大きさ・寿命・色)。速さは粒ごとに外向きの成分を足して使う
	ParticleConfig trailConfig_;

	// --- サウンド ---
	// SoundManagerのキャッシュと共有するBGMデータ(再生中のPCMを有効に保つ)。
	std::shared_ptr<const SoundData> bgmSound_;
	// 再生ハンドル。シーンを抜けるとき(Finalize)にBGMを止めるために保持する。
	// 型は SoundManager::SoundHandle(=uint32_t)。ヘッダにxaudio2.hを引き込まないため素の型で持つ
	uint32_t bgmHandle_ = 0; // 0 = SoundManager::InvalidHandle
	// 隕石が視野を埋めた(画面が真っ黒になった)瞬間の音
	std::shared_ptr<const SoundData> fillSound_;

	OpeningParams params_;
	DepartParams departParams_;
};
