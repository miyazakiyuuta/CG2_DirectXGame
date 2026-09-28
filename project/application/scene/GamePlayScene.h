#pragma once
#include "scene/BaseScene.h"
#include "scene/EditorObject.h"
#include "scene/SceneSerializer.h"

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
class NumberSprite;
class Player;
class Reticle;
class Sprite;
class Stage;
class Skybox;
struct SoundData;
class DebugGrid;
class GPUParticleEmitter;
class SkyCylinder;

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
	// editorObjects_からシーン保存/読込の対象一覧を作る
	std::vector<SceneSerializer::Entry> BuildSerializeEntries() const;

	// 固定オブジェクト+ステージ分からeditorObjects_を作り直す(選択は解除される)。
	// ステージのデータを構造変更(追加/削除/Reload)した後は必ず呼ぶこと(Transformポインタが無効になるため)
	void RebuildEditorObjects();

	// 描画に使うカメラ(デバッグカメラON中はそちらを返す)
	Camera* GetActiveCamera() const;

	// 現在のスコア。撃破数から導出する(内部で加算する状態を持たない=リトライで自動的に0に戻る)
	int GetScore() const;

	/// <summary>
	/// 結果シーンへの遷移を要求する(A-5)。2度目以降の呼び出しは無視される。
	/// 要求後はゲーム進行(レール前進・敵の発生・被弾)を止める:
	/// トランジションが閉じ切るまで、このシーンのUpdateは走り続けるため
	/// </summary>
	/// <param name="sceneName">"CLEAR" または "GAMEOVER"</param>
	void RequestResult(const std::string& sceneName);

	// HUD(HPブロックと操作説明)を作る/更新する/描く。
	// スプライトは深度OFF・αブレンドONなので3Dの上にそのまま重なる
	void InitializeHud();
	void UpdateHud();
	// 被弾の画面着色を進める。ヒットストップ中も演出は動かすので実dtで呼ぶこと
	void UpdateDamageEffect(float deltaTime);
	void DrawHud();

	// stage.jsonのカメラ調整値(FovY/railSpeed/backDistance)をライブ値へ反映する(Apply)。
	// hasCameraがfalse(camera項目の無い旧stage.json)なら何もしない。
	// 初回LoadFromFile後とReload成功後に呼ぶ。逆方向(Capture)はSaveボタン側で行う
	void ApplyCameraFromStage();

#ifdef USE_IMGUI
	// resources配下のモデル一覧を取り直す(選択中のパスは一覧が変わっても可能なら維持する)
	void RescanModelFiles();
#endif

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

	// レール終端の扱い(A-5)。falseなら終端でクリア、trueなら先頭へ戻って周回する。
	// 周回はゲームの仕様ではなく、配置編集中に同じ区間を何度も確認するためのデバッグ用
	//bool railLoop_ = true;
	bool railLoop_ = false;

	// 結果シーンへの遷移を要求済みか。立っている間はゲーム進行を止める
	bool resultRequested_ = false;

	// --- 被弾時の演出 ---
	// ヒットストップ: 残っている間、ゲーム進行へ渡すdtを0にする。
	// タイマー自体と画面の揺れは実dtで進めるので、止まった画面が震える
	float hitStopTimer_ = 0.0f;
	float hitStopDuration_ = 0.08f;
	// 画面の揺れ: 残り時間に比例して振幅が減衰する。
	// レールカメラがTransformを書いた後にオフセットを足すだけなので、レール計算は汚さない
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.35f;
	float shakeStrength_ = 0.5f; // 最大振幅[m]
	// 画面の着色: 被弾直後だけ画面の縁を赤くする。実装済みのポストエフェクト(Vignette)へ
	// 残り時間を流し込むだけで、このシーンは新しいPSOもシェーダも持たない
	float damageEffectTimer_ = 0.0f;
	float damageEffectDuration_ = 0.45f;
	float damageVignettePower_ = 0.85f; // 被弾直後の濃さ(Vignetteのintensityは0〜1)

	// カメラをプレイヤーの何m後方に置くか。railDistance_はプレイヤーの進行度で、
	// カメラはプレイヤーの座標系(接線基準)を forward×この値 だけ下がった位置に派生させる。
	// レール基準点を1つに統一することで、カーブでも画面内の自機位置が固定される
	float cameraBackDistance_ = 10.0f;

	// 入力アクション層(KB/パッド→アクションの対応付け)とプレイヤー
	std::unique_ptr<ActionInput> actionInput_;
	std::unique_ptr<Player> player_;

	// 自弾のプール。所有者はシーン(Playerには参照だけ渡す)。
	std::unique_ptr<BulletManager> bulletManager_;
	// 敵弾のプール。自弾と同じクラスで、違いはBulletManager::Profile(見た目・音・速さ)だけ。
	// 撃つのはEnemySpawner、当たり判定はPlayer、所有はシーンという分担にしてある
	std::unique_ptr<BulletManager> enemyBulletManager_;
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

	// --- サウンド ---
	// SoundManagerのキャッシュと共有するBGMデータ。
	std::shared_ptr<const SoundData> bgmSound_;
	// 再生ハンドルはシーンを抜けるとき(Finalize)にBGMを止めるために保持する。
	// 型は SoundManager::SoundHandle(=uint32_t)。ヘッダにxaudio2.hを引き込まないため素の型で持つ
	uint32_t bgmHandle_ = 0; // 0 = SoundManager::InvalidHandle

	// --- HUD(A-5) ---
	// HPブロック(white1x1を1マス=HP1として並べる)。残量が整数で読み取れるようバー分割にはしない
	std::vector<std::unique_ptr<Sprite>> hpSprites_;
	// 操作説明(画像1枚)。紹介動画の撮影時に消せるようshowHud_で切替
	std::unique_ptr<Sprite> controlsSprite_;
	bool showHud_ = true;

	// スコア表示(画面右上)。撃破数から導出するので、加算のための状態は持たない
	std::unique_ptr<NumberSprite> scoreNumber_;

	// 照準(レティクル)。狙点はPlayerが持っているので、これは表示だけを担当する
	std::unique_ptr<Reticle> reticle_;

	// stage.jsonから構築するステージ配置(静的オブジェクト群)
	std::unique_ptr<Stage> stage_;

	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<GPUParticleEmitter> gpuParticleEmitter_;

	std::unique_ptr<SkyCylinder> skyCylinder_;
};

