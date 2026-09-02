#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "base/SrvManager.h"
#include "base/WinApp.h"
#include "2d/TextureManager.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleManager.h"
#include "effect/GPUParticleEmitter.h"
#include "3d/Skybox.h"
#include "stage/Stage.h"
#include "input/ActionInput.h"
#include "player/Player.h"
#include "bullet/BulletManager.h"
#include "enemy/EnemySpawner.h"
#include "audio/SoundManager.h"
#include "ui/NumberSprite.h"
#include "scene/ResultScene.h"
#include "3d/SkyCylinder.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/DepthBasedOutline.h"
#include "scene/SceneManager.h"
#include "transition/BlindTransition.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include "math/Matrix4x4.h"
#include "utility/Random.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include "debug/EditorPanels.h"
#include "debug/TransformGizmo.h"
#include "stage/StageSerializer.h"
#endif

namespace {
	// シーン配置の保存先(作業ディレクトリ=プロジェクト直下からの相対パス)
	const std::string kScenePath = "resources/scenes/GamePlayScene.json";
	// ステージデータの置き場所(「エディタで作ってゲームが読む」パイプラインの受け渡しファイル)
	const std::string kStagePath = "resources/scenes/stage.json";

	// BGM(魔王魂 cyber43)。利用にあたり「音楽：魔王魂」のクレジット表記が必要なので、
	// ポートフォリオ/ReadMeへの記載を忘れないこと
	const std::string kBgmSound = "resources/sounds/maou_bgm_cyber43.mp3";
	// カテゴリ音量。BGMは背景に回し、SE(射撃・撃破)を前に出す配分
	constexpr float kBgmVolume = 0.10f;
	constexpr float kSeVolume = 0.30f;

	// --- HUDのレイアウト(A-5) ---
	// 1280x720基準で決め、実解像度の倍率を掛けて使う。
	// kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値では置けない
	const std::string kControlsTexture = "resources/ui/controls.png";
	// HPマスの下地。white1x1ではなくwhite2x2を使う:
	// TextureManagerはGenerateMipMapsを通すが、1x1はミップが1段しか作れず
	// E_INVALIDARGでassertに落ちる(DirectXTexMipMaps.cpp: levels <= 1 で弾かれる)
	const std::string kHpBlockTexture = "resources/white2x2.png";
	constexpr float kUiBaseWidth = 1280.0f;
	constexpr float kHpBlockWidth = 46.0f;  // HP1マスの幅[px]
	constexpr float kHpBlockHeight = 16.0f; // HP1マスの高さ[px]
	constexpr float kHpBlockGap = 6.0f;     // マスの間隔[px]
	constexpr float kHpOriginX = 32.0f;     // 左上からの余白[px]
	constexpr float kHpOriginY = 28.0f;
	constexpr float kControlsMarginX = 32.0f; // 操作説明の左下からの余白[px]
	constexpr float kControlsMarginY = 28.0f;
	// 操作説明の縮小率。原寸だと画面幅の6割近くを占めて主役になってしまう
	constexpr float kControlsScale = 0.7f;

	// スコア表示(画面右上)。数字画像は0〜9が等幅で並んだ1枚
	const std::string kNumberTexture = "resources/ui/numbers.png";
	constexpr int kScoreDigits = 6;
	// 1桁の表示サイズ[px](1280x720基準)。切り出し元セルが217.2x257なので、
	// 比率0.85を保って歪ませない。桁間の隙間は画像側が余白を持っているので0でよい
	constexpr float kScoreDigitWidth = 30.0f;
	constexpr float kScoreDigitHeight = 36.0f;
	constexpr float kScoreDigitGap = 0.0f;
	constexpr float kScoreMarginX = 32.0f;     // 右上からの余白[px]
	constexpr float kScoreMarginY = 26.0f;
	// 敵1体撃破あたりの点数
	constexpr int kScorePerEnemy = 100;
}

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

	// パーティクルにこのシーンのアクティブカメラを渡す(初期化・更新・描画はFrameworkが行う)
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	GPUParticleManager::GetInstance()->SetCamera(camera_.get());

	// 敵の撃破演出(A-4の残り)。プリセットとして登録しておき、EnemySpawnerは
	// Emit(名前, 位置, 個数)の1行で発生させる(発生側がconfigを持たない=調整はここ1箇所)。
	// RegisterEffectは冪等なので、リトライでシーンが作り直されても二重登録にならない。
	// 火花(spark)と違い上下も含めた全方向へ飛ばし、重力を弱めて球状の広がりを残す
	ParticleConfig explosionConfig;
	explosionConfig.minScale = { 0.4f, 0.4f, 0.4f };
	explosionConfig.maxScale = { 0.9f, 0.9f, 0.9f };
	explosionConfig.minVelocity = { -6.0f, -6.0f, -6.0f };
	explosionConfig.maxVelocity = { 6.0f, 6.0f, 6.0f };
	explosionConfig.acceleration = { 0.0f, -3.0f, 0.0f };
	explosionConfig.lifeTimeMin = 0.25f; // 短命にして「弾けて消える」印象にする
	explosionConfig.lifeTimeMax = 0.7f;
	explosionConfig.startColor = { 1.0f, 0.9f, 0.5f, 1.0f };
	explosionConfig.endColor = { 1.0f, 0.25f, 0.0f, 0.0f };
	explosionConfig.endScaleRatio = 0.1f; // 縮みながら消える
	ParticleManager::GetInstance()->RegisterEffect(
	EnemySpawner::DefeatEffectName(), "resources/circle.png", explosionConfig, BlendMode::Add);

	// GPUパーティクルのサンプル: 空間を漂う塵(大量・常時放出はGPU側の担当)
	// 定常時の生存数 ≈ emitCount × 平均寿命 ÷ frequency = 1000 × 6 ÷ 0.1 = 約6万粒
	GPUParticleConfig dustConfig;
	dustConfig.minScale = { 0.05f, 0.05f, 0.05f };
	dustConfig.maxScale = { 0.15f, 0.15f, 0.15f };
	dustConfig.minVelocity = { -0.3f, 0.1f, -0.3f };
	dustConfig.maxVelocity = { 0.3f, 0.8f, 0.3f };
	dustConfig.lifeTimeMin = 4.0f;
	dustConfig.lifeTimeMax = 8.0f;
	dustConfig.startColor = { 0.6f, 0.8f, 1.0f, 0.8f };
	dustConfig.endColor = { 0.2f, 0.4f, 1.0f, 0.0f };
	dustConfig.endScaleRatio = 0.5f;
	gpuParticleEmitter_ = std::make_unique<GPUParticleEmitter>("gpuDust", "resources/circle.png", dustConfig);
	gpuParticleEmitter_->SetPosition({ 0.0f, 3.0f, 0.0f });
	gpuParticleEmitter_->SetRadius(15.0f);
	gpuParticleEmitter_->SetEmitCount(1000);
	gpuParticleEmitter_->SetFrequency(0.1f);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(camera_.get());

	// SkyCylinderが使うテクスチャ。
	// モデルとパーティクル用テクスチャは各所有者(Stage/EnemySpawner/BulletManager/Player/
	// ParticleManager)が自分でロードするため、ここでの先読みは不要
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

	// ステージ配置をstage.jsonから構築(必要なモデルはStage側がロードする)
	stage_ = std::make_unique<Stage>();
	stage_->SetCamera(camera_.get());
	stage_->LoadFromFile(kStagePath);
	// カメラ調整値を反映(読み込み失敗時はhasCamera=falseの初期データなので何も起きない)
	ApplyCameraFromStage();

	// プレイヤー(位置は「レール距離+ローカルオフセット」分解。レールはステージ構築後に渡す)
	actionInput_ = std::make_unique<ActionInput>();
	player_ = std::make_unique<Player>();
	player_->Initialize(actionInput_.get());
	player_->SetCamera(camera_.get());
	player_->SetRail(&stage_->GetRail());
	// 当たり判定の相手。StageのメンバはReload/Rebuildでも実体のアドレスが変わらないため
	// 一度渡せば以降も有効(中身だけが作り直される)
	player_->SetStageColliders(&stage_->GetWorldColliders());

	// 自弾。ここでプール分のObject3dを作り切るので、ゲーム中は一切生成/破棄が起きない
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize();
	bulletManager_->SetCamera(camera_.get());
	bulletManager_->SetStageColliders(&stage_->GetWorldColliders());
	player_->SetBulletManager(bulletManager_.get());

	// stage.jsonのSpawnPointを取り込む(Reload時も同じ経路で作り直す)
	enemySpawner_ = std::make_unique<EnemySpawner>();
	enemySpawner_->SetCamera(camera_.get());
	// 被弾判定に使う自弾の一覧の取得元(所有はシーン。敵は参照するだけ)
	enemySpawner_->SetBulletManager(bulletManager_.get());
	enemySpawner_->BuildFromStage(stage_->GetData());

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	skyCylinder_ = std::make_unique<SkyCylinder>();
	skyCylinder_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), "resources/uvChecker.png");
	skyCylinder_->SetCamera(camera_.get());
	skyCylinder_->GetTransform().scale = { 50.0f, 20.0f, 50.0f };
	skyCylinder_->GetTransform().translate = { 0.0f,  -5.0f,  0.0f };

#ifdef USE_IMGUI
	// エディタのモデル選択Comboに出す一覧(実行中にファイルを足したらRescanボタンで取り直す)
	RescanModelFiles();
#endif

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧を構築
	RebuildEditorObjects();

	// 保存済みのシーン配置があれば復元(無ければ上の初期値のまま)
	SceneSerializer::Load(kScenePath, BuildSerializeEntries());

	effectManager_->FindEffect("Monochrome")->enabled = false;
	effectManager_->FindEffect("RadialBlur")->enabled = false;
	//effectManager_->FindEffect("DepthBasedOutline")->enabled = true;
	if (auto* e = effectManager_->FindEffect("DepthBasedOutline")) {
		auto* outline = static_cast<DepthBasedOutline*>(e);
		outline->SetCamera(camera_.get()); // このシーンのアクティブカメラ
		outline->enabled = true;
	}

	// HP表示と操作説明(プレイヤーのMax HPを見るのでplayer_の初期化後に呼ぶ)
	InitializeHud();

	// BGM。カテゴリ音量はBGMを下げてSEを立たせる配分にしてある
	// (個別の音量はBulletManager/EnemySpawner側で更に絞っている)
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::BGM, kBgmVolume);
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::SE, kSeVolume);
	bgmSound_ = SoundManager::GetInstance()->LoadFile(kBgmSound);
	bgmHandle_ = SoundManager::GetInstance()->PlayWave(
		bgmSound_, true, SoundManager::SoundCategory::BGM);
}

void GamePlayScene::InitializeHud() {
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kUiBaseWidth;

	TextureManager::GetInstance()->LoadTexture(kHpBlockTexture);
	TextureManager::GetInstance()->LoadTexture(kControlsTexture);

	// HPはマス目で表す。Max HP分だけ作り、残量に応じて色を変える(個数は毎フレーム変えない)
	hpSprites_.clear();
	for (int i = 0; i < player_->GetMaxHp(); ++i) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(SpriteCommon::GetInstance(), kHpBlockTexture);
		sprite->SetSize({ kHpBlockWidth * uiScale, kHpBlockHeight * uiScale });
		sprite->SetPos({
			(kHpOriginX + static_cast<float>(i) * (kHpBlockWidth + kHpBlockGap)) * uiScale,
			kHpOriginY * uiScale });
		hpSprites_.push_back(std::move(sprite));
	}

	// 操作説明は左下。Initialize直後のサイズ=テクスチャの実寸なので、それを倍率で拡大する
	controlsSprite_ = std::make_unique<Sprite>();
	controlsSprite_->Initialize(SpriteCommon::GetInstance(), kControlsTexture);
	const Vector2 controlsSize = controlsSprite_->GetSize();
	const float controlsScale = uiScale * kControlsScale;
	controlsSprite_->SetSize({ controlsSize.x * controlsScale, controlsSize.y * controlsScale });
	controlsSprite_->SetPos({
		kControlsMarginX * uiScale,
		static_cast<float>(WinApp::kClientHeight)
			- controlsSize.y * controlsScale - kControlsMarginY * uiScale });
	controlsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.75f }); // 主張しすぎないよう少し透かす

	// スコアは右上。桁数固定なので右端から逆算して左上座標を決める
	const float digitWidth = kScoreDigitWidth * uiScale;
	const float digitGap = kScoreDigitGap * uiScale;
	const float totalWidth = digitWidth * kScoreDigits + digitGap * (kScoreDigits - 1);
	scoreNumber_ = std::make_unique<NumberSprite>();
	scoreNumber_->Initialize(kNumberTexture, kScoreDigits);
	scoreNumber_->SetDigitSize({ digitWidth, kScoreDigitHeight * uiScale }, digitGap);
	scoreNumber_->SetPosition({
		static_cast<float>(WinApp::kClientWidth) - totalWidth - kScoreMarginX * uiScale,
		kScoreMarginY * uiScale });
}

int GamePlayScene::GetScore() const {
	return static_cast<int>(enemySpawner_->GetDefeatedCount()) * kScorePerEnemy;
}

void GamePlayScene::UpdateHud() {
	// 残っているマスは白、失ったマスは暗い赤。個数ではなく色で減少を見せる
	for (size_t i = 0; i < hpSprites_.size(); ++i) {
		const bool remaining = static_cast<int>(i) < player_->GetHp();
		hpSprites_[i]->SetColor(remaining
			? Vector4{ 1.0f, 1.0f, 1.0f, 0.9f }
			: Vector4{ 0.35f, 0.08f, 0.08f, 0.6f });
		hpSprites_[i]->Update();
	}
	controlsSprite_->Update();
	scoreNumber_->SetValue(GetScore());
	scoreNumber_->Update();
}

void GamePlayScene::DrawHud() {
	if (!showHud_) {
		return;
	}
	// 3D描画の後に呼ぶ。スプライトのPSOは深度OFF・αブレンドONなので手前に重なる
	SpriteCommon::GetInstance()->CommonDrawSetting();
	for (const std::unique_ptr<Sprite>& sprite : hpSprites_) {
		sprite->Draw();
	}
	controlsSprite_->Draw();
	scoreNumber_->Draw(); // 数字画像が未配置なら何も描かない
}

void GamePlayScene::RequestResult(const std::string& sceneName) {
	if (resultRequested_) {
		return; // 遷移は1度きり(SceneManager側も予約中は無視するが、進行停止はこのフラグで行う)
	}
	resultRequested_ = true;
	// 結果画面へ表示用のスコアを預ける(ChangeSceneはシーン名しか渡せないため)
	ResultScene::SetScore(GetScore());
	SceneManager::GetInstance()->ChangeScene(sceneName, std::make_unique<BlindTransition>());
}

void GamePlayScene::Finalize() {
	// 結果画面へ鳴り続けないよう止める。
	// リトライで新しいGamePlaySceneが作られると、そちらのInitializeが鳴らし直す
	SoundManager::GetInstance()->StopWave(bgmHandle_);
	bgmHandle_ = 0;
}

void GamePlayScene::Update(float deltaTime) {
	// ヒットストップ: 残っている間はゲーム進行へ渡すdtを0にする。
	// タイマー自体は実dtで消化する(止めると永久に解除されない)。
	// カメラ・HUD・画面の揺れ、およびFrameworkが回すパーティクルは実dtのまま進むので、
	// 「自機と敵と弾だけが止まり、爆発は流れ続ける」形になる
	float gameDelta = deltaTime;
	if (hitStopTimer_ > 0.0f) {
		hitStopTimer_ = (std::max)(hitStopTimer_ - deltaTime, 0.0f);
		gameDelta = 0.0f;
	}

	// F1でデバッグカメラON/OFF(ImGuiのDebug Cameraウィンドウでも切替可)
	if (Input::GetInstance()->IsTriggerKey(DIK_F1)) {
		debugCamera_->SetActive(!debugCamera_->IsActive());
	}
#ifdef USE_IMGUI
	// Fキーで選択中オブジェクトへフォーカス(Sceneビュー上でのみ反応)
	if (debugCamera_->IsActive() && selectedIndex_ >= 0 &&
		Input::GetInstance()->IsSceneViewHovered() && Input::GetInstance()->IsTriggerKey(DIK_F)) {
		const Transform* target = editorObjects_[selectedIndex_].transform;
		float radius = (std::max)({ target->scale.x, target->scale.y, target->scale.z });
		debugCamera_->FocusOn(target->translate, radius);
	}
#endif
	debugCamera_->Update(deltaTime);

	// デバッグカメラON中は描画に使うカメラを差し替える(ゲームカメラのTransformは汚さない)
	Camera* activeCamera = GetActiveCamera();
	ParticleManager::GetInstance()->SetCamera(activeCamera);
	GPUParticleManager::GetInstance()->SetCamera(activeCamera);
	stage_->SetCamera(activeCamera);
	player_->SetCamera(activeCamera);
	bulletManager_->SetCamera(activeCamera);
	enemySpawner_->SetCamera(activeCamera);
	skyCylinder_->SetCamera(activeCamera);
	if (auto* effect = effectManager_->FindEffect("DepthBasedOutline")) {
		static_cast<DepthBasedOutline*>(effect)->SetCamera(activeCamera);
	}

	skyCylinder_->Update();

	// レールカメラ: railDistance_はプレイヤーの進行度。カメラはプレイヤーの座標系を
	// 接線方向にcameraBackDistance_だけ下がった位置に置き、接線の向きで進行方向を向く。
	// rail(d-後方距離)を評価せずforwardで直接下がるのが肝(カメラとプレイヤーが同一の
	// レール座標系を共有し、カーブでも画面内の自機位置が数学的に固定される)。
	// camera_->Update()より前にTransformを書き、同フレームのview行列へ反映させる
	if (railCameraActive_) {
		const CatmullRomSpline& rail = stage_->GetRail();
		if (rail.GetTotalLength() > 0.0f) {
			// 結果表示を要求した後は前進を止める(トランジションが閉じる間もここは走り続けるため)。
			// カメラのTransform更新は止めない: 止めると凍結中のフレームでビューが更新されなくなる
			if (!resultRequested_) {
				railDistance_ += railSpeed_ * gameDelta;

				if (railLoop_) {
					// 周回はデバッグ用(配置編集中に同じ区間を何度も確認する)
					const float previousDistance = railDistance_;
					railDistance_ = std::fmod(railDistance_, rail.GetTotalLength());
					// 一周して進行度が巻き戻ったら敵も未発生に戻す(周回するたび同じ敵が出る)
					if (railDistance_ < previousDistance) {
						enemySpawner_->Reset();
						bulletManager_->Clear(); // 前周の弾がレール先頭に取り残されないようにする
					}
				} else if (railDistance_ >= rail.GetTotalLength()) {
					// レール終端に到達 = クリア条件(A-5)。
					// 終端を超えた分は切り捨てる(スプラインの評価範囲外へ出さない)
					railDistance_ = rail.GetTotalLength();
					RequestResult("CLEAR");
				}
			}

			Vector3 tangent = rail.GetTangentByDistance(railDistance_);
			Vector3 cameraPosition = rail.GetPositionByDistance(railDistance_) - tangent * cameraBackDistance_;

			// 被弾時の画面の揺れ。レール上の基準位置を出した「後」にオフセットを足すだけなので、
			// レール座標系の計算(自機位置・狙点・敵の判定)には一切影響しない。
			// 減衰は実dtなので、ヒットストップで止まっている間も震え続ける
			if (shakeTimer_ > 0.0f) {
				shakeTimer_ = (std::max)(shakeTimer_ - deltaTime, 0.0f);
				const float amplitude = shakeStrength_ * (shakeTimer_ / shakeDuration_);
				// ワールド軸ではなくカメラのローカル右/上へずらす
				// (ワールド軸だとカーブの向きによって揺れ方が変わって見えるため)
				const Matrix4x4 rotation = Matrix4x4::Rotate(camera_->GetTransform().rotate);
				const Vector3 right = rotation.Transform({ 1.0f, 0.0f, 0.0f });
				const Vector3 up = rotation.Transform({ 0.0f, 1.0f, 0.0f });
				cameraPosition += right * Random::GetFloat(-amplitude, amplitude)
					+ up * Random::GetFloat(-amplitude, amplitude);
			}
			camera_->SetTranslate(cameraPosition);

			// 接線→オイラー角。回転規約(行ベクトル×Rx→Ry→Rz)では
			// forward = (cos(pitch)·sin(yaw), -sin(pitch), cos(pitch)·cos(yaw)) なので逆算する
			float yaw = std::atan2(tangent.x, tangent.z);
			float pitch = std::atan2(-tangent.y, std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z));
			camera_->SetRotate({ pitch, yaw, 0.0f }); // バンク(roll)はしない
		}
	}

	camera_->Update();
	camera_->TransferToGPU();

	// プレイヤーより先に更新する: Stage::Updateがワールドコライダーを作り直すため、
	// この順なら同フレームの編集(ギズモ移動等)がそのまま当たり判定に反映される
	stage_->Update(gameDelta);

	// 被弾の検出用に更新前のHPを控える。ダメージ源は地形(Player内部)と敵(この下)の2つに
	// 分かれているので、ダメージ源を問わず拾える「HPが減ったか」で判定する
	const int hpBeforeUpdate = player_->GetHp();

	// プレイヤーはレール上のrailDistance_地点そのもの(進行はレールカメラ側で行うため供給のみ)
	player_->SetRailDistance(railDistance_);
	player_->Update(gameDelta);

	// プレイヤーの直後に更新する: 同フレームに発射された弾もこのフレーム分だけ進むため、
	// 発射の1フレーム目が銃口に張り付いて見えることがない
	bulletManager_->Update(gameDelta);

	// 進行度がSpawnPointのrailDistanceを超えたら敵が発生する。
	// 弾の更新後に呼ぶことで、弾が今フレーム分進んだ位置で被弾判定できる
	enemySpawner_->Update(railDistance_, gameDelta, player_->GetWorldPosition(),
		player_->GetCollisionRadius());

	// 敵との接触ダメージ(A-5)。接触の判定は敵側、無敵時間の管理はPlayer側にあるので、
	// シーンは「触れていたら殴る」という繋ぎ込みだけを持つ。
	// 地形との接触はPlayer::Updateが自分で適用している
	if (!resultRequested_ && enemySpawner_->IsPlayerHit()) {
		player_->TakeDamage(1);
	}

	// 今フレームHPが減っていたらヒットストップと画面の揺れを起こす。
	// 地形・敵のどちらで減ったかを区別せず、Playerの無敵時間で弾かれた分も自動的に除外される
	if (player_->GetHp() < hpBeforeUpdate) {
		hitStopTimer_ = hitStopDuration_;
		shakeTimer_ = shakeDuration_;
	}

	// HPが尽きたらゲームオーバー(A-5)。クリア判定はレール終端側にある
	if (!resultRequested_ && player_->IsDead()) {
		RequestResult("GAMEOVER");
	}

	// HUDの色と頂点を更新(HPが確定した後)
	UpdateHud();

	DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 10.0f, 20, { 1.0f,1.0f,1.0f,0.5f });

	// レール曲線の可視化(stage.json手編集→Reloadの確認用)。曲線=赤の折れ線、制御点=黄の球
	const CatmullRomSpline& rail = stage_->GetRail();
	const std::vector<Vector3>& railPoints = rail.GetControlPoints();
	if (railPoints.size() >= 2) {
		// 1区間16分割でサンプリングして折れ線として描く
		const int division = static_cast<int>(railPoints.size() - 1) * 16;
		Vector3 prevPos = rail.GetPosition(0.0f);
		for (int i = 1; i <= division; ++i) {
			float t = static_cast<float>(i) / static_cast<float>(division);
			Vector3 pos = rail.GetPosition(t);
			DebugRenderer::GetInstance()->AddLine(prevPos, pos, { 1.0f,0.2f,0.2f,1.0f });
			prevPos = pos;
		}
	}
	for (const Vector3& point : railPoints) {
		DebugRenderer::GetInstance()->AddSphere(point, 0.5f, { 1.0f,1.0f,0.2f,1.0f });
	}

	// ステージコライダーの可視化(stage.jsonのcolliderが当たり判定になっていることの確認用)。
	// 通常=緑、プレイヤーが接触中は全体を赤にして当たった瞬間が分かるようにする
	if (showColliders_) {
		const Vector4 colliderColor = player_->IsHit()
			? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
			: Vector4{ 0.2f, 1.0f, 0.4f, 1.0f };
		for (const AABB& aabb : stage_->GetWorldColliders()) {
			const Vector3 center = (aabb.min + aabb.max) * 0.5f;
			const Vector3 size = aabb.max - aabb.min;
			DebugRenderer::GetInstance()->AddBox3D(center, size, colliderColor);
		}
		// プレイヤーの判定形状(球)も出して、どこで当たるかを見えるようにする
		DebugRenderer::GetInstance()->AddSphere(
			player_->GetWorldPosition(), player_->GetCollisionRadius(), colliderColor);
	}

	// 照準の可視化(レティクル(A-2)が入るまでの確認用)。
	// 銃口→狙点の線と狙点の球を出すと、Aim Convergenceを変えたときに弾道が
	// レール中心へ寄っていく様子がそのまま目で追える
	if (showAimLine_) {
		const Vector4 aimColor = { 0.4f, 1.0f, 1.0f, 1.0f };
		DebugRenderer::GetInstance()->AddLine(
			player_->GetMuzzlePosition(), player_->GetAimPoint(), aimColor);
		DebugRenderer::GetInstance()->AddSphere(player_->GetAimPoint(), 0.5f, aimColor);
	}

	// SpawnPoint(敵の発生地点)の可視化。未発生=黄、発生済み=グレー。
	// 「レール上のどの進行度で出るか」が分かるよう、発生地点とレール上の該当点を線で結ぶ
	if (showSpawnPoints_) {
		for (const EnemySpawner::SpawnPoint& spawnPoint : enemySpawner_->GetSpawnPoints()) {
			const Vector4 color = spawnPoint.spawned
				? Vector4{ 0.5f, 0.5f, 0.5f, 1.0f }
				: Vector4{ 1.0f, 0.9f, 0.2f, 1.0f };
			const Vector3& position = spawnPoint.transform.translate;
			DebugRenderer::GetInstance()->AddBox3D(position, { 2.0f, 2.0f, 2.0f }, color);
			DebugRenderer::GetInstance()->AddSphere(position, 0.6f, color);
			DebugRenderer::GetInstance()->AddLine(
				position, rail.GetPositionByDistance(spawnPoint.railDistance), color);
		}
	}
}

void GamePlayScene::Draw() {
	//skybox_->Draw(*camera_);
	skyCylinder_->Draw();

	stage_->Draw();
	player_->Draw();
	bulletManager_->Draw();
	enemySpawner_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*GetActiveCamera());

	// HUDは3Dの最後(HPと操作説明)
	DrawHud();
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	// Hierarchy: オブジェクト一覧+選択、シーンファイル操作
	ImGui::Begin("Hierarchy");
	EditorPanels::DrawHierarchy(editorObjects_, selectedIndex_);

	// ステージオブジェクトの追加/複製/削除(stage.jsonの管轄分のみ。C++直書きの固定オブジェクトは対象外)
	ImGui::SeparatorText("Stage Objects");
	const bool stageObjectSelected = selectedIndex_ >= static_cast<int>(fixedEditorObjectCount_);
	const size_t stageIndex = stageObjectSelected ? selectedIndex_ - fixedEditorObjectCount_ : 0;

	// Addで使うモデルの選択(Rescanボタンが横に並ぶため、Comboは残り幅の6割に抑える)
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
	const char* addModelPreview =
		modelFiles_.empty() ? "(no models)" : modelFiles_[addModelIndex_].c_str();
	if (ImGui::BeginCombo("##addModel", addModelPreview)) {
		for (int i = 0; i < static_cast<int>(modelFiles_.size()); ++i) {
			if (ImGui::Selectable(modelFiles_[i].c_str(), addModelIndex_ == i)) {
				addModelIndex_ = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan")) {
		RescanModelFiles();
	}

	ImGui::BeginDisabled(modelFiles_.empty());
	if (ImGui::Button("Add")) {
		StageData::ObjectData objectData;
		objectData.model = modelFiles_[addModelIndex_];
		// 名前はモデルファイル名から拡張子を除いたものを既定にする("fence/fence.obj" → "fence")
		objectData.name = std::filesystem::path(objectData.model).stem().string();
		// 今見ている場所に出るよう、アクティブカメラの前方に生成する
		const Transform& cameraTransform = GetActiveCamera()->GetTransform();
		Vector3 forward = Matrix4x4::Rotate(cameraTransform.rotate).Transform({ 0.0f, 0.0f, 1.0f });
		objectData.transform.translate = cameraTransform.translate + forward * 10.0f;

		size_t newIndex = stage_->AddObject(std::move(objectData));
		RebuildEditorObjects();
		selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + newIndex);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	// 敵の発生地点を追加する。staticと同じobjects[]に入るのでHierarchy・ギズモがそのまま使える
	if (ImGui::Button("Add Spawn")) {
		StageData::ObjectData objectData;
		objectData.type = StageData::ObjectType::Spawn;
		objectData.name = "spawn";
		objectData.enemy = EnemySpawner::EnemyTypes().front();
		// 発生タイミングの既定は「今の進行度の少し先」。置いた直後に発生してしまうのを避ける
		objectData.railDistance = railDistance_ + 10.0f;
		const Transform& cameraTransform = GetActiveCamera()->GetTransform();
		Vector3 forward = Matrix4x4::Rotate(cameraTransform.rotate).Transform({ 0.0f, 0.0f, 1.0f });
		objectData.transform.translate = cameraTransform.translate + forward * 10.0f;

		size_t newIndex = stage_->AddObject(std::move(objectData));
		enemySpawner_->BuildFromStage(stage_->GetData());
		RebuildEditorObjects();
		selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + newIndex);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!stageObjectSelected);
	if (ImGui::Button("Duplicate")) {
		size_t newIndex = stage_->DuplicateObject(stageIndex);
		RebuildEditorObjects();
		selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + newIndex);
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete")) {
		stage_->RemoveObject(stageIndex);
		RebuildEditorObjects(); // 選択解除もここで行われる
	}
	ImGui::EndDisabled();

	// ステージ配置のファイル操作。Reloadはstage.jsonの内容で上書き(未保存の編集は破棄される)
	ImGui::SeparatorText("Stage File");
	if (ImGui::Button("Save##stage")) {
		// Capture: カメラ調整値のライブ値をStageDataへ書き戻してから保存する
		// (Save=Capture+書き出し。所有者はシーンなので、Camera APIを呼ぶのはここ)
		StageData& data = stage_->GetData();
		data.hasCamera = true;
		data.camera = { camera_->GetFovY(), railSpeed_, cameraBackDistance_ };
		StageSerializer::Save(kStagePath, data);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload##stage")) {
		// 失敗時(JSON破損等)はLoadFromFileが現状維持するので、一覧の作り直しも不要
		if (stage_->LoadFromFile(kStagePath)) {
			ApplyCameraFromStage(); // カメラ調整値もファイルの内容へ戻す
			enemySpawner_->BuildFromStage(stage_->GetData()); // SpawnPointもファイルの内容で作り直す
			RebuildEditorObjects(); // 構造変更でTransformポインタが無効になるため必須(選択解除も行われる)
		}
	}

	// レールカメラの操作。毎フレームゲームカメラを上書きするため、
	// 配置編集(ギズモ/Inspector)したいときはここでOFFにする
	ImGui::SeparatorText("Rail Camera");
	ImGui::Checkbox("Active##railCamera", &railCameraActive_);
	ImGui::SameLine();
	if (ImGui::Button("Reset##railCamera")) {
		railDistance_ = 0.0f; // 周回を待たずに先頭から確認し直す用
		enemySpawner_->Reset(); // 敵も未発生に戻し、発生の瞬間を何度でも確認できるようにする
		bulletManager_->Clear(); // 飛行中の弾も消す(先頭に戻った瞬間の状態を揃える)
	}
	// ONにすると終端で先頭へ戻る(クリア遷移せずに配置を何度も見たいとき用)
	ImGui::Checkbox("Loop (debug)", &railLoop_);
	ImGui::DragFloat("Speed", &railSpeed_, 0.1f, 0.0f, 100.0f, "%.1f m/s");
	ImGui::DragFloat("Camera Back", &cameraBackDistance_, 0.1f, 0.0f, 50.0f, "%.1f m");
	ImGui::Text("Distance: %.1f / %.1f m", railDistance_, stage_->GetRail().GetTotalLength());

	// 勝敗まわり(A-5)。遷移待ちの間はゲーム進行が止まっていることをここで確認できる
	ImGui::SeparatorText("Game");
	ImGui::Text("HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
	ImGui::Text("Result: %s", resultRequested_ ? "requested (frozen)" : "playing");
	ImGui::Checkbox("Show HUD", &showHud_);

	// 被弾演出の調整。ヒットストップは長すぎると操作が引っかかるので0.05〜0.15が目安
	ImGui::SeparatorText("Damage Feedback");
	ImGui::DragFloat("Hit Stop", &hitStopDuration_, 0.005f, 0.0f, 0.5f, "%.3f s");
	ImGui::DragFloat("Shake Time", &shakeDuration_, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::DragFloat("Shake Power", &shakeStrength_, 0.05f, 0.0f, 5.0f, "%.2f m");
	if (ImGui::Button("Test Damage")) {
		hitStopTimer_ = hitStopDuration_;
		shakeTimer_ = shakeDuration_;
	}
	if (ImGui::Button("Force Clear")) {
		RequestResult("CLEAR");
	}
	ImGui::SameLine();
	if (ImGui::Button("Force GameOver")) {
		RequestResult("GAMEOVER");
	}

	// 当たり判定の可視化切替(コライダーのワイヤーボックス+プレイヤーの判定球)
	ImGui::SeparatorText("Collision");
	ImGui::Checkbox("Show Colliders", &showColliders_);
	ImGui::Text("Active Colliders: %zu", stage_->GetWorldColliders().size());
	// 銃口→狙点の線と狙点の球(レティクル実装前に弾道を確認するため)
	ImGui::Checkbox("Show Aim Line", &showAimLine_);

	// SpawnPointの可視化と発生状況(進行度トリガーが効いていることの確認用)
	ImGui::SeparatorText("Spawn Points");
	ImGui::Checkbox("Show Spawn Points", &showSpawnPoints_);
	ImGui::SameLine();
	if (ImGui::Button("Respawn")) {
		enemySpawner_->Reset();
	}
	ImGui::Text("Spawned: %zu / %zu",
		enemySpawner_->GetSpawnedCount(), enemySpawner_->GetSpawnPoints().size());

	ImGui::SeparatorText("Scene File");
	if (ImGui::Button("Save")) {
		SceneSerializer::Save(kScenePath, BuildSerializeEntries());
	}
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		SceneSerializer::Load(kScenePath, BuildSerializeEntries());
	}
	ImGui::End();

	// Inspector: ギズモ操作モード+選択中オブジェクトの編集
	ImGui::Begin("Inspector");
	TransformGizmo::DrawOperationSelector();
	EditorPanels::DrawInspector(editorObjects_, selectedIndex_);
	ImGui::End();

	// デバッグカメラの切替・速度・感度
	debugCamera_->DrawImGui();

	// プレイヤーのオフセット・速度・射撃/照準・アクション押下状態
	player_->DrawImGui();

	// 弾の生存数・速度・寿命・判定半径
	bulletManager_->DrawImGui();

	// 敵の生存数・撃破数・HP・移動のパラメータ
	enemySpawner_->DrawImGui();

	// Sceneビューのクリックで選択を更新し、選択中の対象にギズモを表示
	// (デバッグカメラON中はその視点で描画されているため、ピック・ギズモも同じカメラで行う)
	Camera* activeCamera = GetActiveCamera();
	TransformGizmo::PickBySceneClick(*activeCamera, editorObjects_, selectedIndex_);
	if (selectedIndex_ >= 0) {
		TransformGizmo::Manipulate(*activeCamera, editorObjects_[selectedIndex_]);
	}

#endif
}

std::vector<SceneSerializer::Entry> GamePlayScene::BuildSerializeEntries() const {
	// 固定オブジェクトだけが保存対象。ステージ分はstage.jsonの管轄なのでここには含めない
	std::vector<SceneSerializer::Entry> entries;
	entries.reserve(fixedEditorObjectCount_);
	for (size_t i = 0; i < fixedEditorObjectCount_; ++i) {
		entries.push_back({ editorObjects_[i].name, editorObjects_[i].transform });
	}
	return entries;
}

void GamePlayScene::RebuildEditorObjects() {
	// C++直書きの固定オブジェクト。追加したらここに1行足すだけで全機能に反映される。
	// SkyCylinderは全天を覆うため、Cameraは実体が見えないためクリック選択の対象外
	editorObjects_.clear();
	editorObjects_.push_back({ "SkyCylinder", &skyCylinder_->GetTransform(), false });
	editorObjects_.push_back({ "Camera", &camera_->GetTransform(), false });
	editorObjects_.back().scaleEditable = false; // カメラのscaleはビュー行列を歪ませるため編集させない

#ifdef USE_IMGUI
	// 型別のInspector追加UI(ImGui呼び出しを含むためDebug構成のみ)
	editorObjects_.back().drawInspector = [this]() {
		float fovY = camera_->GetFovY();
		if (ImGui::DragFloat("FovY", &fovY, 0.01f)) {
			camera_->SetFovY(fovY);
		}
	};
#endif
	fixedEditorObjectCount_ = editorObjects_.size();

	// ステージ分を後ろへ連結(transformはStageData直指し。編集はStage::Updateが実体へ反映する)
	std::vector<EditorObject> stageObjects = stage_->BuildEditorObjects();
	editorObjects_.insert(editorObjects_.end(),
		std::make_move_iterator(stageObjects.begin()), std::make_move_iterator(stageObjects.end()));

#ifdef USE_IMGUI
	// ステージ分にはモデル差し替えComboと無効フラグ切替を付ける
	// (StageはImGui非依存のまま、UIは登録側=シーンが持つ)。
	// どちらも構造変更ではないためこの一覧は作り直し不要で、選択もそのまま維持される
	for (size_t i = fixedEditorObjectCount_; i < editorObjects_.size(); ++i) {
		const size_t stageObjectIndex = i - fixedEditorObjectCount_;
		editorObjects_[i].onSetDisabled = [this, i, stageObjectIndex](bool disabled) {
			stage_->SetObjectDisabled(stageObjectIndex, disabled);
			editorObjects_[i].pickable = !disabled; // 実体が消えるのでクリック選択の対象からも外す
			// SpawnPointの無効化も同じ意味(データは残すがゲームには出さない)にするため作り直す
			enemySpawner_->BuildFromStage(stage_->GetData());
		};
		editorObjects_[i].drawInspector = [this, stageObjectIndex]() {
			StageData::ObjectData& data = stage_->GetData().objects[stageObjectIndex];

			if (data.type == StageData::ObjectType::Spawn) {
				// 敵の発生地点: モデルではなく「種別」と「発生する進行度」を編集する。
				// 位置はtransform(ギズモ)で置く。どちらもSave/Reloadの往復に乗る
				ImGui::TextUnformatted("Type: Spawn (enemy spawn point)");
				if (ImGui::BeginCombo("Enemy", data.enemy.c_str())) {
					for (const std::string& type : EnemySpawner::EnemyTypes()) {
						if (ImGui::Selectable(type.c_str(), type == data.enemy) && type != data.enemy) {
							data.enemy = type;
							enemySpawner_->BuildFromStage(stage_->GetData()); // 種別変更を即反映
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::DragFloat("RailDistance", &data.railDistance, 0.5f, 0.0f,
					stage_->GetRail().GetTotalLength(), "%.1f m")) {
					enemySpawner_->BuildFromStage(stage_->GetData());
				}
				return; // spawnにモデル選択・コライダーは不要
			}

			// 参照ではなくコピーで持つ(SetObjectModelが元の文字列を書き換えるため)
			const std::string current = data.model;
			if (ImGui::BeginCombo("Model", current.c_str())) {
				for (const std::string& file : modelFiles_) {
					if (ImGui::Selectable(file.c_str(), file == current) && file != current) {
						stage_->SetObjectModel(stageObjectIndex, file);
					}
				}
				ImGui::EndCombo();
			}

			// コライダー編集。StageDataを直接書き換えるので、既存のSave/Reloadの往復に
			// そのまま乗る(Serializerはcollider読み書き済み)。判定への反映はStage::Updateが行う。
			// 要素数を変えない編集なのでeditorObjects_の作り直しは不要
			StageData::ObjectData& objectData = stage_->GetData().objects[stageObjectIndex];
			ImGui::Checkbox("Collider", &objectData.hasCollider);
			if (objectData.hasCollider) {
				// AABBは軸平行のためTransformのrotateは判定に影響しない(Stage::UpdateWorldColliders)
				ImGui::DragFloat3("C.Center", &objectData.collider.center.x, 0.05f);
				ImGui::DragFloat3("C.Size", &objectData.collider.size.x, 0.05f, 0.0f, 1000.0f);
			}
		};
	}
#endif

	// 一覧が変わったので選択は解除(古いindexは別物を指しうる)
	selectedIndex_ = -1;
}

void GamePlayScene::ApplyCameraFromStage() {
	const StageData& data = stage_->GetData();
	if (!data.hasCamera) {
		return; // camera項目の無い旧stage.json。ライブ値をデフォルトへ巻き戻さない
	}
	camera_->SetFovY(data.camera.fovY);
	railSpeed_ = data.camera.railSpeed;
	cameraBackDistance_ = data.camera.backDistance;
}

Camera* GamePlayScene::GetActiveCamera() const {
	return debugCamera_->IsActive() ? debugCamera_->GetCamera() : camera_.get();
}

#ifdef USE_IMGUI
void GamePlayScene::RescanModelFiles() {
	// 再スキャンで一覧の並びが変わっても同じモデルを指し続けるよう、選択はindexではなくパスで引き継ぐ
	std::string selectedPath = "sphere.obj"; // 初回の既定
	if (addModelIndex_ >= 0 && addModelIndex_ < static_cast<int>(modelFiles_.size())) {
		selectedPath = modelFiles_[addModelIndex_];
	}

	modelFiles_ = ModelManager::GetInstance()->ScanModelFiles();

	addModelIndex_ = 0;
	for (int i = 0; i < static_cast<int>(modelFiles_.size()); ++i) {
		if (modelFiles_[i] == selectedPath) {
			addModelIndex_ = i;
			break;
		}
	}
}
#endif

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;
