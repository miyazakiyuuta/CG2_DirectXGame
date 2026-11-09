#pragma once
#include "scene/BaseScene.h"
#include "scene/EditorObject.h"
#include "scene/SceneSerializer.h"
#include "transition/BlindTransition.h"

#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

class ActionInput;
class BulletManager;
class Camera;
class DebugCamera;
class EnemySpawner;
class GameOverSequence;
class IntroCamera;
class NumberSprite;
class Player;
class ProgressGauge;
class Reticle;
class SectionBanner;
class Sprite;
class Stage;
class StageProgress;
class Skybox;
struct SoundData;
class DebugGrid;

class GamePlayScene : public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	void DrawImGui() override;

	GamePlayScene();

	~GamePlayScene() override;

private:

	enum class Mode {
		Play,   // ゲーム進行中
		Edit,   // エディタモード(ギズモ/Inspector/保存)
	};


	// editorObjects_からシーン保存/読込の対象一覧を作る
	std::vector<SceneSerializer::Entry> BuildSerializeEntries() const;

	// 固定オブジェクト+ステージ分からeditorObjects_を作り直す(選択は解除される)。
	// ステージのデータを構造変更(追加/削除/Reload)した後は必ず呼ぶこと(Transformポインタが無効になるため)
	void RebuildEditorObjects();

	// 描画に使うカメラ(デバッグカメラON中はそちらを返す)
	Camera* GetActiveCamera() const;

	// 現在のスコア。撃破数とPERFECTの区間数から導出する(内部で加算する状態を持たない=リトライで自動的に0に戻る)
	int GetScore() const;

	// このプレイで受けたダメージの合計(最大HP - 残りHP)。ランク(駆除報告)の評価に使う
	int GetDamageTaken() const;

	/// <summary>
	/// ロックオンのつなぎ込み。チャージ中は照準の近く(Lock Assistの角度以内)にいる敵を吸い付く候補に選び、
	/// 満タンになったらその候補をPlayerにロックさせる。ロック中の敵が居なくなったら外す。
	/// 判定は敵側(EnemySpawner)、ロックの保持はPlayerで、ここは両者をつなぐだけ(レティクルの色替えと同じ分担)。
	/// 敵の更新(撃破・移動)の後に呼ぶこと
	/// </summary>
	void UpdateLockOn();

	/// <summary>
	/// 結果シーンへの遷移を要求する(A-5)。2度目以降の呼び出しは無視される。
	/// 要求後はゲーム進行(レール前進・敵の発生・被弾)を止める:
	/// トランジションが閉じ切るまで、このシーンのUpdateは走り続けるため。
	/// エディタのPlayボタンから始めたプレイなら、遷移せずEditへ戻る
	/// </summary>
	/// <param name="sceneName">"CLEAR" または "GAMEOVER"</param>
	/// <param name="startState">遷移の帯の始まり方。ゲームオーバー演出で画面が真っ黒になっているときはClosed
	/// (閉じた状態から始めてすぐ切り替える。タイトル→本編の遷移と同じ)</param>
	void RequestResult(const std::string& sceneName,
		BlindTransition::StartState startState = BlindTransition::StartState::Open);

	/// <summary>
	/// ゲームオーバー演出を始める(HPが尽きたときと、ImGuiのForce GameOverから呼ぶ)。
	/// 演出が真っ黒になり切ったらRequestResult("GAMEOVER")へ進む。
	/// 演出中はレールを止め、操作を止め、自機の代わりに墜落する機体を描く
	/// </summary>
	void StartGameOver();

	// HUD(HPブロックと操作説明)を作る/更新する/描く。
	// スプライトは深度OFF・αブレンドONなので3Dの上にそのまま重なる
	void InitializeHud();
	// イントロ中は消しておき、終わったらフェードインさせる(操作開始の合図を兼ねる)。透明度は実dtで進める
	void UpdateHud(float deltaTime);
	// 被弾の画面着色を進める。ヒットストップ中も演出は動かすので実dtで呼ぶこと
	void UpdateDamageEffect(float deltaTime);
	// 画面を揺らし始める(被弾・爆発)。今の揺れの方が強ければ何もしない(弱い揺れで強い揺れを打ち消さない)
	void StartShake(float power);
	void DrawHud();

	// stage.jsonのカメラ調整値(FovY/railSpeed/backDistance)をライブ値へ反映する(Apply)。
	// hasCameraがfalse(camera項目の無い旧stage.json)なら何もしない。
	// 初回LoadFromFile後とReload成功後に呼ぶ。逆方向(Capture)はSaveボタン側で行う
	void ApplyCameraFromStage();

	// EnterPlay・EnterEditで呼ぶ。レール距離・敵発生・敵弾・自機・自弾・被弾演出・resultRequested_を戻す
	// 開始時の状態に戻す・設定は残す。イントロは止めるだけで、始めるのはInitializeとEnterPlay
	// (Stopでは再生しないため)
	void ResetGameState();

	// エディタのPlay/Restartボタンから呼ぶ。ゲーム状態を開始時に戻してから、本番と同じくイントロからPlayに入る
	// (途中からの再開ではない)。配置と設定は残る。視点はゲームカメラに戻す
	void EnterPlay();

	// エディタのStopボタンと、エディタから始めたプレイの出口(RequestResult)から呼ぶ。
	// ゲーム状態を開始時に戻してからEditに入る(Play中に行った配置、設定は残る)。
	// ゲームの進行は止まり、視点は編集用のデバッグカメラに切り替わる
	void EnterEdit();

#ifdef USE_IMGUI
	// resources配下のモデル一覧を取り直す(選択中のパスは一覧が変わっても可能なら維持する)
	void RescanModelFiles();

	/// <summary>
	/// "Sections"ウィンドウ: 区間(ステージの区切り)の一覧・追加・削除・開始距離の編集。
	/// StageDataを直接書き換えるので、Hierarchyの Save(stage) でstage.jsonへ残る。
	/// どの操作も「開始距離の昇順・先頭は0」の約束(StageData::sections)を崩さない
	/// </summary>
	void DrawSectionsWindow();
#endif

	Mode mode_ = Mode::Play;
	// このプレイがエディタのPlayボタン(EnterPlay)から始まったか。
	// trueの間は、プレイの終わり(GAMEOVER/CLEAR)で結果画面へ遷移せずEditに戻る。
	// ゲームの状態ではなく「どう始まったか」の情報なので、ResetGameStateでは戻さない。
	// 本番(タイトル・リトライ)はシーンが新しく作られてfalseから始まるので、戻す必要がない
	bool startedFromEditor_ = false;

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧と選択状態
	std::vector<EditorObject> editorObjects_;
	int selectedIndex_ = -1; // -1 = 選択なし
	// 先頭からこの個数分がC++直書きの固定オブジェクト(GamePlayScene.jsonの保存対象)。
	// 以降はステージ分(stage.jsonの管轄なのでシーン保存には含めない)
	size_t fixedEditorObjectCount_ = 0;

#ifdef USE_IMGUI
	// エディタのモデル選択Comboに出す一覧(resources配下のスキャン結果)と、Addで使う選択位置
	std::vector<std::string> modelFiles_;
	int addModelIndex_ = 0;
#endif

	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	// レールカメラ: レール上の現在距離[m]と速度[m/s]。
	// activeがfalseの間はゲームカメラのTransformを上書きしない(ギズモ/Inspectorでの編集用)
	float railDistance_ = 0.0f;
	float railSpeed_ = 10.0f;
	bool railCameraActive_ = true;

	// レール終端の扱い。falseなら終端でクリア、trueなら先頭へ戻って周回する。
	// 周回はゲームの仕様ではなく、配置編集中に同じ区間を何度も確認するためのデバッグ用
	//bool railLoop_ = true;
	bool railLoop_ = false;

	// 結果シーンへの遷移を要求済みか。立っている間はゲーム進行を止める
	bool resultRequested_ = false;

	// --- 被弾時の演出 ---
	// ヒットストップ: 残っている間、ゲーム進行へ渡すdeltaTimeを0にする。
	// タイマー自体と画面の揺れは実deltaTimeで進めるので、止まった画面が震える
	float hitStopTimer_ = 0.0f;
	float hitStopDuration_ = 0.08f;
	// 画面の揺れ: 残り時間に比例して振幅が減衰する。
	// レールカメラがTransformを書いた後にオフセットを足すだけなので、レール計算は汚さない
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.35f;
	float shakeStrength_ = 0.5f; // 被弾時の最大振幅[m]
	// 今の揺れの最大振幅[m]。被弾と爆発で強さが違うので、揺らし始めたとき(StartShake)の強さを覚えておく
	float shakePower_ = 0.0f;
	// 画面の着色: 被弾直後だけ画面の縁を赤くする。実装済みのポストエフェクト(Vignette)へ
	// 残り時間を流し込むだけで、このシーンは新しいPSOもシェーダも持たない
	float damageEffectTimer_ = 0.0f;
	float damageEffectDuration_ = 0.45f;
	float damageVignettePower_ = 0.85f; // 被弾直後の濃さ(Vignetteのintensityは0〜1)

	// --- チャージ弾の爆発の手応え ---
	// 被弾と同じヒットストップ・画面の揺れを、弱めに使う(被弾の方が強いと区別できるように)
	float explosionHitStopDuration_ = 0.05f;
	float explosionShakeStrength_ = 0.25f; // 最大振幅[m]

	// カメラをプレイヤーの何m後方に置くか。railDistance_はプレイヤーの進行度で、
	// カメラはプレイヤーの座標系(接線基準)を forward×この値 だけ下がった位置に派生させる。
	// レール基準点を1つに統一することで、カーブでも画面内の自機位置が固定される
	float cameraBackDistance_ = 10.0f;

	// ゲーム開始のカメラ演出(自機の前方斜め上から回り込み、レールカメラの定位置に着いたら操作開始)。
	// 再生中はレールを進めず、自機の移動・射撃・被弾を止め、HUDを消す
	std::unique_ptr<IntroCamera> introCamera_;

	// ゲームオーバーの演出(致命的な被弾 → 制御不能 → 遠ざかる → 爆発 → 暗転)。
	// 時間倍率・墜落する機体・パーティクル・ポストエフェクトは演出が持ち、
	// シーンはレール・操作・HUD・画面の揺れ・結果画面への遷移をつなぐ(イントロと同じ分担)
	std::unique_ptr<GameOverSequence> gameOverSequence_;

	// 入力アクション層(KB/パッド→アクションの対応付け)とプレイヤー
	std::unique_ptr<ActionInput> actionInput_;
	std::unique_ptr<Player> player_;

	// 自弾のプール。所有者はシーン(Playerには参照だけ渡す)。
	std::unique_ptr<BulletManager> bulletManager_;
	// 敵弾のプール。自弾と同じクラスで、違いはBulletManager::Profile(見た目・音・速さ)だけ。
	// 撃つのはEnemySpawner、当たり判定はPlayer、所有はシーンという分担にしてある
	std::unique_ptr<BulletManager> enemyBulletManager_;
	// チャージ弾のプール(容量1)。これも同じクラスで、違いはProfile(大きさ・ダメージ・爆発の半径)だけ。
	// 撃つのはPlayer、敵への当たり判定と爆発の巻き込みはEnemySpawner、所有はシーン
	std::unique_ptr<BulletManager> chargeBulletManager_;
	// 狙点と弾道のデバッグ表示
	bool showAimLine_ = false;

	// ステージコライダーとプレイヤー判定球のデバッグ表示(全構成。ImGuiのチェックボックスで切替)
	// 既定はOFF: デバッグ描画はDebugRendererが全構成で描くため、ONのままだと
	// 紹介動画やRelease配布にも緑の判定ボックスが映り込む
	bool showColliders_ = false;
	// SpawnPoint(敵の発生地点)のデバッグ表示
	bool showSpawnPoints_ = false;
	// ワールドグリッドとレール曲線(赤の折れ線・黄の制御点)のデバッグ表示
	bool showRail_ = false;

	// stage.jsonのSpawnPointから敵を発生させる(進行度トリガー)
	std::unique_ptr<EnemySpawner> enemySpawner_;

	// 区間ごとの集計(駆除数・取り逃し・PERFECT)とクリア時のランク。
	// 区切り(stage.json)と敵の結末(EnemySpawner)を毎フレーム数え直すだけなので、リセットの必要がない
	std::unique_ptr<StageProgress> stageProgress_;

	// --- サウンド ---
	// SoundManagerのキャッシュと共有するBGMデータ。
	std::shared_ptr<const SoundData> bgmSound_;
	// 再生ハンドルはシーンを抜けるとき(Finalize)にBGMを止めるために保持する。
	// 型は SoundManager::SoundHandle(=uint32_t)。ヘッダにxaudio2.hを引き込まないため素の型で持つ
	uint32_t bgmHandle_ = 0; // 0 = SoundManager::InvalidHandle
	// ゲームオーバー演出でBGMをフェードアウトさせたか。フェードし切ると再生が止まるので、
	// このシーンのまま続ける(エディタのPlayから始めたプレイがEditへ戻る)ときはResetGameStateで鳴らし直す
	bool bgmFadedOut_ = false;

	// --- HUD(A-5) ---
	// HPブロック(white1x1を1マス=HP1として並べる)。残量が整数で読み取れるようバー分割にはしない
	std::vector<std::unique_ptr<Sprite>> hpSprites_;
	// 操作説明(画像1枚)。紹介動画の撮影時に消せるようshowHud_で切替
	std::unique_ptr<Sprite> controlsSprite_;
	bool showHud_ = true;
	// HUD全体の透明度(0〜1)。イントロ中は0、終わったら1へフェードインする
	float hudAlpha_ = 0.0f;

	// スコア表示(画面右上)。撃破数から導出するので、加算のための状態は持たない
	std::unique_ptr<NumberSprite> scoreNumber_;

	// 進行ゲージ(画面上部の中央)。今どこにいるか・区間の区切り・PERFECTの区間・終点の女王を1本の帯で見せる。
	// 中身はstageProgress_の集計とレールの進行度で、これは表示だけを担当する
	std::unique_ptr<ProgressGauge> progressGauge_;

	// 区間の結果表示(PERFECT / SECTION CLEAR と「n / m」)。区間が確定したフレームにShowする
	std::unique_ptr<SectionBanner> sectionBanner_;

	// 照準(レティクル)。狙点はPlayerが持っているので、これは表示だけを担当する
	std::unique_ptr<Reticle> reticle_;
	// チャージ中に照準を引き寄せる先の敵の番号(0=なし)。UpdateLockOnが毎フレーム選び直し、
	// UpdateHudがレティクルへ渡す。ロックした後はロック中の敵の方へ引き寄せる
	uint32_t lockCandidateId_ = 0;

	// stage.jsonから構築するステージ配置(静的オブジェクト群)
	std::unique_ptr<Stage> stage_;

	std::unique_ptr<Skybox> skybox_;
};

