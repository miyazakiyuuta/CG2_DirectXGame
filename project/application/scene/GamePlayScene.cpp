#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "base/WinApp.h"
#include "2d/TextureManager.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleManager.h"
#include "3d/Skybox.h"
#include "stage/Stage.h"
#include "input/ActionInput.h"
#include "player/Player.h"
#include "bullet/BulletManager.h"
#include "enemy/EnemySpawner.h"
#include "progress/StageProgress.h"
#include "camera/IntroCamera.h"
#include "direction/GameOverSequence.h"
#include "direction/ClearSequence.h"
#include "audio/SoundManager.h"
#include "ui/NumberSprite.h"
#include "ui/Reticle.h"
#include "ui/ProgressGauge.h"
#include "ui/SectionBanner.h"
#include "scene/ResultScene.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/Vignette.h"
#include "scene/SceneManager.h"
#include "transition/BlindTransition.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include "math/Matrix4x4.h"
#include "utility/Random.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <cstring>
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

	// --- HUDのレイアウト ---
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
	// 操作説明の濃さ。主張しすぎないよう少し透かす
	constexpr float kControlsAlpha = 0.75f;
	// イントロが終わってからHUDが出揃うまでの時間[秒]。HUDが現れること自体が操作開始の合図になる
	constexpr float kHudFadeInDuration = 0.4f;
	// ゲームオーバー演出が始まってからHUDが消え切るまでの時間[秒]。操作できなくなったことを画面から伝える
	constexpr float kHudFadeOutDuration = 0.3f;
	// ゲームオーバー演出でBGMを消していく時間[秒]。演出(約6.75秒)のうち、爆発(約4.25秒)の直前で消え切り、
	// 爆発と暗転を静かに見せる。演出の段階の長さを変えたら合わせて見直す
	constexpr float kGameOverBgmFadeDuration = 4.0f;

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
	// 区間を取り逃しなし(PERFECT)で終えたときのボーナス。区間ごとの目標を達成した手応えを点数でも返す
	constexpr int kScorePerPerfectSection = 500;

	// チャージ弾の演出に使うパーティクルのプリセット名。登録(RegisterEffect)と、
	// 弾のProfileへの指定はどちらもこのシーンで行う(名前を2か所に直書きしないよう定数にまとめる)
	const std::string kChargeTrailEffect = "chargeTrail";
	const std::string kChargeExplosionEffect = "chargeExplosion";
	// チャージ弾が消える(爆発する)ときの音。撃破音と同じ素材だが、
	// 爆発したフレームは撃破音を鳴らさない(EnemySpawner)ので同じ音が重ならない
	const std::string kChargeExplosionSound = "resources/sounds/explosion.mp3";

	// 区間の最短の長さ[m]。区切りどうしをこれより近づけない(長さほぼ0の、敵が入りようのない区間を作らない)
	constexpr float kMinSectionLength = 1.0f;
	// 区間の区切りの目印(Show Railのとき、レール上に描く)。レールの赤・制御点の黄と見分けるため水色にする
	const Vector4 kSectionMarkerColor = { 0.2f, 0.9f, 1.0f, 1.0f }; // Vector4のコンストラクタはconstexprでないのでconst
	constexpr float kSectionMarkerHeight = 6.0f; // 区切りから上へ伸ばす線の長さ[m](遠くからでも位置が分かるように)
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

	// チャージ弾の尾。弾と同じ黄緑で、小さく短命にして「光の筋」に見せる。
	// 弾の通り道に置いていくだけなので、速度はごく小さくしてその場に残す
	ParticleConfig chargeTrailConfig;
	chargeTrailConfig.minScale = { 0.25f, 0.25f, 0.25f };
	chargeTrailConfig.maxScale = { 0.45f, 0.45f, 0.45f };
	chargeTrailConfig.minVelocity = { -0.6f, -0.6f, -0.6f };
	chargeTrailConfig.maxVelocity = { 0.6f, 0.6f, 0.6f };
	chargeTrailConfig.lifeTimeMin = 0.2f;
	chargeTrailConfig.lifeTimeMax = 0.4f;
	chargeTrailConfig.startColor = { 0.7f, 1.0f, 0.4f, 0.9f };
	chargeTrailConfig.endColor = { 0.2f, 0.8f, 0.1f, 0.0f };
	chargeTrailConfig.endScaleRatio = 0.2f; // 細くなりながら消える
	ParticleManager::GetInstance()->RegisterEffect(
		kChargeTrailEffect, "resources/circle.png", chargeTrailConfig, BlendMode::Add);

	// チャージ弾の爆発。撃破の爆発(上)より大きく速く広げ、爆発の半径(6m)が目で分かる広がりにする
	// (速さ±14m/s×寿命0.3〜0.6秒 ≒ 半径4〜8m)。色は弾と同じ黄緑から橙へ変え、チャージ弾の爆発だと分かるようにする
	ParticleConfig chargeExplosionConfig;
	chargeExplosionConfig.minScale = { 0.6f, 0.6f, 0.6f };
	chargeExplosionConfig.maxScale = { 1.4f, 1.4f, 1.4f };
	chargeExplosionConfig.minVelocity = { -14.0f, -14.0f, -14.0f };
	chargeExplosionConfig.maxVelocity = { 14.0f, 14.0f, 14.0f };
	chargeExplosionConfig.acceleration = { 0.0f, -2.0f, 0.0f };
	chargeExplosionConfig.lifeTimeMin = 0.3f;
	chargeExplosionConfig.lifeTimeMax = 0.6f;
	chargeExplosionConfig.startColor = { 0.9f, 1.0f, 0.6f, 1.0f }; 
	chargeExplosionConfig.endColor = { 1.0f, 0.4f, 0.0f, 0.0f };
	chargeExplosionConfig.endScaleRatio = 0.2f;
	ParticleManager::GetInstance()->RegisterEffect(
		kChargeExplosionEffect, "resources/circle.png", chargeExplosionConfig, BlendMode::Add);

	// 巣の全滅(クリア演出)で、落ちた敵が消えるときの煙。撃破の爆発(上)と違い光らないので、
	// αに丸の形を持たせたsmoke.pngをαブレンドで使う(加算だと重なるほど白く光ってしまう)。
	// 登録はここ・発生はEnemySpawner、という撃破の爆発と同じ分担
	ParticleConfig exterminateConfig;
	exterminateConfig.minScale = { 1.0f, 1.0f, 1.0f };
	exterminateConfig.maxScale = { 2.0f, 2.0f, 2.0f };
	exterminateConfig.minVelocity = { -2.0f, 0.5f, -2.0f };
	exterminateConfig.maxVelocity = { 2.0f, 3.0f, 2.0f };
	exterminateConfig.lifeTimeMin = 0.6f;
	exterminateConfig.lifeTimeMax = 1.2f;
	exterminateConfig.startColor = { 0.6f, 0.6f, 0.55f, 0.6f };
	exterminateConfig.endColor = { 0.3f, 0.3f, 0.3f, 0.0f };
	exterminateConfig.endScaleRatio = 2.5f; // 広がりながら薄くなる
	ParticleManager::GetInstance()->RegisterEffect(
		EnemySpawner::ExterminateEffectName(), "resources/smoke.png", exterminateConfig, BlendMode::Alpha);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(camera_.get());

	// モデルとパーティクル用テクスチャは各所有者(Stage/EnemySpawner/BulletManager/Player/
	// ParticleManager)が自分でロードするため、ここでの先読みは不要

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
	spawnerRailRevision_ = stage_->GetRailRevision();

	// 区間ごとの集計。区切りと敵の結末は毎フレームUpdateで受け取るので、ここでは作るだけ
	stageProgress_ = std::make_unique<StageProgress>();

	// 敵弾。自弾と同じBulletManagerの2個目で、違いはProfile(見た目・音・速さ)だけ。
	// 撃つのはEnemySpawner、当たったかを見るのはPlayerで、ここは所有と配線だけを持つ
	enemyBulletManager_ = std::make_unique<BulletManager>();
	BulletManager::Profile enemyBulletProfile;
	enemyBulletProfile.color = { 1.0f, 0.5f, 0.15f, 1.0f }; // 自弾(水色)と一目で区別できる暖色
	// 自弾(80m/s)より遅くする: 見てから避けられる速さでないと、撃たれること自体が理不尽になる。
	// 半径0.5と自機0.5の和=1.0mに対し1フレームの移動は0.5m(60fps)なのですり抜けない
	enemyBulletProfile.speed = 30.0f;
	enemyBulletProfile.lifeTime = 5.0f; // 射程150m(遠くの敵の弾も自機まで届く)
	enemyBulletProfile.radius = 0.5f;   // 自弾より大きく、飛んでくるのが分かるサイズ
	enemyBulletProfile.shotVolume = 0.18f; // 敵の数だけ鳴るので自弾より控える
	enemyBulletProfile.debugName = "Enemy Bullet"; // 自弾のウィンドウと分ける(同名は合体する)
	enemyBulletManager_->Initialize(64, enemyBulletProfile);
	enemyBulletManager_->SetCamera(camera_.get());
	enemyBulletManager_->SetStageColliders(&stage_->GetWorldColliders());
	enemySpawner_->SetEnemyBulletManager(enemyBulletManager_.get());
	player_->SetEnemyBulletManager(enemyBulletManager_.get());

	// チャージ弾(長押しで溜めて、離すと撃つ)。BulletManagerの3つ目で、違いはProfileだけ。
	// 容量を1にして「同時に飛ぶのは1発まで」をプールの空きで表す(Playerは空きが無い間チャージを溜めない)
	chargeBulletManager_ = std::make_unique<BulletManager>();
	BulletManager::Profile chargeBulletProfile;
	chargeBulletProfile.color = { 0.6f, 1.0f, 0.3f, 1.0f }; // 自弾(水色)とも敵弾(橙)とも違う黄緑
	// 自弾(80m/s)より遅くする: 溜めた弾が飛んでいく様子を目で追えるようにする
	chargeBulletProfile.speed = 50.0f;
	chargeBulletProfile.lifeTime = 2.5f; // 射程125m
	chargeBulletProfile.radius = 0.8f;   // 溜めた分だけ大きく見せる
	chargeBulletProfile.damage = 3;      // 今の敵(HP2〜3)をどれも直撃1発で倒せる
	chargeBulletProfile.explosionRadius = 6.0f; // 当たった周りの敵も巻き込む
	// ロック中に撃ったときの追尾の強さ。50m/sで半径約10mの円を描ける曲がり方
	// (弱すぎると、近くの敵の周りを曲がり切れずに回ってしまう)
	chargeBulletProfile.homingTurnRate = 300.0f;
	// 演出: 飛んでいる間の尾と、消えるとき(敵・地形・射程切れ)の爆発。
	// どちらのプリセットもこの関数の先頭で登録済み(未登録のままEmitするとassertで止まる)
	chargeBulletProfile.trailEffect = kChargeTrailEffect;
	chargeBulletProfile.trailRate = 120.0f;
	chargeBulletProfile.impactEffect = kChargeExplosionEffect;
	chargeBulletProfile.impactParticleCount = 120;
	chargeBulletProfile.impactSound = kChargeExplosionSound;
	chargeBulletProfile.impactVolume = 0.8f;
	chargeBulletProfile.shotVolume = 0.6f; // 専用の発射音が入るまでは、自弾と同じ音を大きめに鳴らして区別する
	chargeBulletProfile.debugName = "Charge Bullet";
	chargeBulletManager_->Initialize(1, chargeBulletProfile);
	chargeBulletManager_->SetCamera(camera_.get());
	chargeBulletManager_->SetStageColliders(&stage_->GetWorldColliders());
	player_->SetChargeBulletManager(chargeBulletManager_.get());
	enemySpawner_->SetChargeBulletManager(chargeBulletManager_.get());
	// 追尾先の位置は「敵の番号→位置」の窓口で引く。BulletManagerは敵の型を知らないまま追尾できる。
	// enemySpawner_もこのシーンが持っているので、弾より先に消えて窓口が宙に浮くことはない
	chargeBulletManager_->SetTargetResolver(
		[spawner = enemySpawner_.get()](uint32_t targetId, Vector3& outPosition) {
			return spawner->TryGetEnemyPosition(targetId, outPosition);
		});

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

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

	// HP表示と操作説明(プレイヤーのMax HPを見るのでplayer_の初期化後に呼ぶ)
	InitializeHud();

	// BGM。カテゴリ音量はBGMを下げてSEを立たせる配分にしてある
	// (個別の音量はBulletManager/EnemySpawner側で更に絞っている)
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::BGM, kBgmVolume);
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::SE, kSeVolume);
	bgmSound_ = SoundManager::GetInstance()->LoadFile(kBgmSound);
	bgmHandle_ = SoundManager::GetInstance()->PlayWave(
		bgmSound_, true, SoundManager::SoundCategory::BGM);

	// ゲームオーバーの演出。墜落する機体のモデル・煙と火花・爆発音はここで用意し、HPが尽きたときに始める
	gameOverSequence_ = std::make_unique<GameOverSequence>();
	gameOverSequence_->Initialize(effectManager_);
	gameOverSequence_->SetCamera(camera_.get());

	// クリアの演出。亡骸の器と爆発の粒はここで用意し、女王を倒したときに始める
	clearSequence_ = std::make_unique<ClearSequence>();
	clearSequence_->Initialize(effectManager_);
	clearSequence_->SetCamera(camera_.get());

	// ゲーム開始のイントロ。タイトルから・結果画面のリトライから・(Debugで)直接起動のどれで入っても
	// シーンはここで作られるので、開始はこの1箇所で済む(エディタのPlayボタンはEnterPlayが始める)
	introCamera_ = std::make_unique<IntroCamera>();
	introCamera_->Start();
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
	// 色(濃さ)はHUDのフェードインに合わせてUpdateHudで毎フレーム決める

	// 照準。位置は毎フレーム狙点から決まるので、ここでは組み立てるだけ
	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize();

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

	// 進行ゲージは上部の中央。位置と大きさはゲージ側が決める(区間の数で部品の数が変わるため)
	progressGauge_ = std::make_unique<ProgressGauge>();
	progressGauge_->Initialize();

	// 区間の結果表示。出すのは区間が確定したとき(Update)なので、ここでは作るだけ
	sectionBanner_ = std::make_unique<SectionBanner>();
	sectionBanner_->Initialize();
}

int GamePlayScene::GetScore() const {
	return static_cast<int>(enemySpawner_->GetDefeatedCount()) * kScorePerEnemy
		+ stageProgress_->GetPerfectCount() * kScorePerPerfectSection;
}

int GamePlayScene::GetDamageTaken() const {
	return player_->GetMaxHp() - player_->GetHp();
}

void GamePlayScene::StartShake(float power) {
	// 今の揺れの方が強ければ何もしない。被弾で大きく揺れている最中に爆発が起きても、
	// 弱い揺れで上書きして被弾の揺れが急に小さくならないようにする
	const float currentAmplitude = shakeDuration_ > 0.0f ? shakePower_ * (shakeTimer_ / shakeDuration_) : 0.0f;
	if (currentAmplitude >= power) {
		return;
	}
	shakePower_ = power;
	shakeTimer_ = shakeDuration_;
}

void GamePlayScene::UpdateLockOn() {
	// ロック中の敵が居なくなった(撃破・Reset)ら外す。
	// 押し続けていれば、すぐ下の判定で別の敵をロックし直せる
	Vector3 lockPosition{};
	if (player_->GetLockTargetId() != 0 &&
		!enemySpawner_->TryGetEnemyPosition(player_->GetLockTargetId(), lockPosition)) {
		player_->SetLockTarget(0);
	}

	// 吸い付く候補は、チャージ中(溜め始め〜満タン)でまだロックしていない間だけ毎フレーム選び直す。
	// 状態として持ち越さず毎フレーム決め直すので、Resetや撃破で候補が残ることがない
	lockCandidateId_ = 0;
	if (player_->GetChargeRatio() <= 0.0f || player_->GetLockTargetId() != 0) {
		return; // 溜めていない・もうロックしている(ロック中はロックした敵へ吸い付く)
	}

	// 視線 = カメラ → 狙点。カメラを通る直線は画面上では1点(=レティクルの位置)に重なるので、
	// この直線からの角度は「画面上でレティクルからどれだけ離れて見えるか」と一致する。
	// 銃口から伸ばさないのは、カメラと自機の位置がずれていて、遠い敵ほど見た目と判定が食い違うため。
	// カメラは描画に使っている方(デバッグカメラON中はそちら)にして、レティクルの見え方とそろえる。
	// 狙点は引き寄せる前の(自機の操作どおりの)点を使う。引き寄せた照準で選び直すと、
	// 照準が動くたびに候補が変わる堂々巡りになるため
	const Vector3& eye = GetActiveCamera()->GetTranslate();
	const Vector3 toAim = player_->GetAimPoint() - eye;
	if (toAim.LengthSquared() < 1.0e-6f) {
		return; // 狙点がカメラと重なっていて向きが決まらない
	}
	lockCandidateId_ = enemySpawner_->FindEnemyInCone(
		eye, Vector3::Normalized(toAim), player_->GetLockAssistAngle(), player_->GetLockRange());

	// 満タンになったら、引き寄せていた候補をそのままロックする(見えていた吸い付き先と、ロックする敵を一致させる)
	if (player_->CanLockOn() && lockCandidateId_ != 0) {
		player_->SetLockTarget(lockCandidateId_);
	}
}

void GamePlayScene::UpdateDamageEffect(float deltaTime) {
	// ゲームオーバー演出・クリア演出の間は、Vignetteの持ち主は演出の方(赤い縁や暗転・白い閃光は演出が決める)。
	// 1フレームに2か所から書くと、後に書いた方の値だけが残ってしまうため、ここは何もしない
	if (gameOverSequence_->IsActive() || clearSequence_->IsActive()) {
		return;
	}

	// 被弾から一定時間だけ、画面の縁を赤く染める。
	// 「実装済みのポストエフェクトをゲームの状況へ繋ぐ」だけなので、
	// このシーンが持つのはタイマーと濃さの2値だけで済んでいる
	IPostEffect* effect = effectManager_->FindEffect("Vignette");
	if (!effect) {
		return; // 登録が外れていても落とさない(演出は無くてもゲームは成立する)
	}

	if (damageEffectTimer_ > 0.0f) {
		damageEffectTimer_ = (std::max)(damageEffectTimer_ - deltaTime, 0.0f);
	}
	const float ratio = damageEffectDuration_ > 0.0f
		? std::clamp(damageEffectTimer_ / damageEffectDuration_, 0.0f, 1.0f)
		: 0.0f;

	// 残り0で効果ごと切る。シーンを出るときに掛かりっぱなしにならないよう、
	// ON/OFFの判断もこの関数に閉じ込めている
	effect->enabled = ratio > 0.0f;
	if (!effect->enabled) {
		return;
	}

	Vignette* vignette = static_cast<Vignette*>(effect);
	vignette->SetColor({ 1.0f, 0.0f, 0.0f }); // 被弾=赤
	vignette->SetIntensity(damageVignettePower_ * ratio); // 直後が最も濃く、時間で抜ける
	vignette->SetRadius(0.22f);   // 中心寄りから染めて、被弾が一目で分かる濃さにする
	vignette->SetSoftness(0.55f); // 境目を出さずに中心へ向けて薄くする
	vignette->SetPower(1.4f);
	// 縦横の補正なし(画面の形に沿った楕円)。Vignetteは全シーン共有で、他のシーン(タイトルの暗転など)が
	// 変えた値のまま来ることがあるので、使う側が毎回すべての値を決める
	vignette->SetAspect(1.0f);
}

void GamePlayScene::UpdateHud(float deltaTime) {
	// イントロ中は消しておき、終わったらフェードインさせる。
	// イントロ中はカメラが自機の後ろにいないので、狙点を投影したレティクルも見当違いの所に出てしまう。
	// Edit中も消す: 編集中はHP・スコア・操作説明・レティクルが画面に重なって邪魔になるため
	// (Playに入るとイントロが始まり、終わった後にこの下のフェードインで戻る)
	if (introCamera_->IsPlaying() || mode_ == Mode::Edit) {
		hudAlpha_ = 0.0f;
	} else if (gameOverSequence_->IsActive() || clearSequence_->IsActive()) {
		// ゲームオーバー演出の間は消していく(一瞬で消すと、致命的な被弾でHPが0になった様子が見えない)。
		// クリア演出も同じ: 撃破の瞬間のスコアとゲージを一度見せてから、演出だけの画面にする
		hudAlpha_ = (std::max)(hudAlpha_ - deltaTime / kHudFadeOutDuration, 0.0f);
	} else {
		hudAlpha_ = (std::min)(hudAlpha_ + deltaTime / kHudFadeInDuration, 1.0f);
	}

	// 残っているマスは白、失ったマスは暗い赤。個数ではなく色で減少を見せる
	for (size_t i = 0; i < hpSprites_.size(); ++i) {
		const bool remaining = static_cast<int>(i) < player_->GetHp();
		Vector4 color = remaining
			? Vector4{ 1.0f, 1.0f, 1.0f, 0.9f }
			: Vector4{ 0.35f, 0.08f, 0.08f, 0.6f };
		color.w *= hudAlpha_;
		hpSprites_[i]->SetColor(color);
		hpSprites_[i]->Update();
	}
	controlsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, kControlsAlpha * hudAlpha_ });
	controlsSprite_->Update();
	scoreNumber_->SetValue(GetScore());
	scoreNumber_->SetColor({ 1.0f, 1.0f, 1.0f, hudAlpha_ });
	scoreNumber_->Update();

	// 進行ゲージ。区間の集計は敵の更新の後(Update内)で済んでいるので、このフレームの結果がそのまま出る
	progressGauge_->SetAlpha(hudAlpha_);
	progressGauge_->Update(stageProgress_->GetSections(), railDistance_, stage_->GetRail().GetTotalLength(),
		enemySpawner_->HasGoal());

	// 区間の結果表示。表示時間はヒットストップで止めないよう実dtで進める
	sectionBanner_->SetAlpha(hudAlpha_);
	sectionBanner_->Update(deltaTime);

	// 照準。狙点は弾と共有している点なので、レティクルの位置＝弾の到達点になる。
	// 描画に使っているカメラで投影する(デバッグカメラON中はそちらの見え方に合わせる)
	reticle_->SetAlpha(hudAlpha_);
	reticle_->SetAimingAtEnemy(enemySpawner_->IsAimedAt(player_->GetAimPoint()));
	// チャージの溜まり具合とロック中の敵。判定と状態の保持はPlayer・EnemySpawnerが済ませていて、
	// ここは「どう見せるか」を持つReticleへ結果を渡すだけ(照準の色替えと同じ分担)
	reticle_->SetCharge(player_->GetChargeRatio());
	Vector3 lockCenter{};
	float lockRadius = 0.0f;
	if (enemySpawner_->TryGetEnemySphere(player_->GetLockTargetId(), lockCenter, lockRadius)) {
		reticle_->SetLockTarget(player_->GetLockTargetId(), lockCenter, lockRadius);
		// ロック中は、ロックした敵の上へ照準を吸い付けたままにする
		reticle_->SetAssistTarget(lockCenter, 1.0f);
	} else {
		reticle_->ClearLockTarget();
		// チャージ中は、溜まり具合に合わせて候補の敵へ照準を引き寄せていく(満タンでぴったり重なる)
		Vector3 candidatePosition{};
		if (enemySpawner_->TryGetEnemyPosition(lockCandidateId_, candidatePosition)) {
			reticle_->SetAssistTarget(candidatePosition, player_->GetChargeRatio());
		} else {
			reticle_->ClearAssistTarget();
		}
	}
	// 脈打ちとロック枠の動きはHUDのフェードインと同じく実dtで進める(ヒットストップで止めない)
	reticle_->Update(player_->GetAimPoint(), *GetActiveCamera(), deltaTime);
}

void GamePlayScene::DrawHud() {
	if (!showHud_ || hudAlpha_ <= 0.0f) {
		return; // 透明なら描いても見えない(イントロ中)
	}
	// 3D描画の後に呼ぶ。スプライトのPSOは深度OFF・αブレンドONなので手前に重なる
	SpriteCommon::GetInstance()->CommonDrawSetting();
	for (const std::unique_ptr<Sprite>& sprite : hpSprites_) {
		sprite->Draw();
	}
	controlsSprite_->Draw();
	scoreNumber_->Draw(); // 数字画像が未配置なら何も描かない
	progressGauge_->Draw();
	sectionBanner_->Draw(); // 表示中でなければ何も描かない
	reticle_->Draw();     // 狙点がカメラ背面なら何も描かない
}

void GamePlayScene::StartGameOver() {
	if (gameOverSequence_->IsActive()) {
		return; // 演出は1度きり(演出中にForce GameOverを押しても始め直さない)
	}
	// Force GameOverをイントロ中に押した場合に備えて止める(イントロのカメラが演出の見送りを上書きしないように)
	introCamera_->Stop();

	// 煙を出す被弾箇所は、最後に当たった相手の位置から決める(分からなければ演出側の決まった点)
	Vector3 hitSource{};
	const bool hasHitSource = player_->TryGetLastHitSource(hitSource);
	gameOverSequence_->Start(player_->GetWorldPosition(), player_->GetRotate(), hasHitSource ? &hitSource : nullptr);

	// 致命的な被弾の揺れ。通常の被弾(このフレームのHP減少で同時に起きている)より強いので、StartShakeが上書きする
	StartShake(gameOverSequence_->GetParams().impactShake);

	// BGMを消していく。消え切ると再生が止まるので、このシーンのまま続けるときはResetGameStateが鳴らし直す
	SoundManager::GetInstance()->FadeOut(bgmHandle_, kGameOverBgmFadeDuration);
	bgmFadedOut_ = true;
}

void GamePlayScene::StartClear() {
	if (clearSequence_->IsActive()) {
		return; // 演出は1度きり
	}

	// 亡骸を置く場所は、女王が倒れた瞬間の姿(EnemySpawnerが撃破の処理の中で控えている)。
	// 取れないことは起きないはずだが、取れなければ演出を飛ばして今までどおり結果画面へ進む(クリアできなくなるよりよい)
	EnemySpawner::GoalPose pose;
	if (!enemySpawner_->TryGetGoalDefeatPose(pose)) {
		RequestResult("CLEAR");
		return;
	}

	// 時間が止まる瞬間。飛んでいる弾はすべて消す:
	// 敵弾が止まった画面に残らないように、また自弾が固まった敵を倒して集計(駆除数・ランク)を変えないように
	bulletManager_->Clear();
	enemyBulletManager_->Clear();
	chargeBulletManager_->Clear();
	// 残りの敵はその場で固める(大爆発の瞬間に、段階の変化を見てExterminateAllで落とす)
	enemySpawner_->FreezeAll();

	clearSequence_->Start(EnemySpawner::ModelPathOf(pose.enemy), pose.transform, pose.center, pose.radius);
	StartShake(clearSequence_->GetParams().impactShake);
	// 音は今回は触らない(BGM・撃破音は今までどおり。大爆発の音やファンファーレは後で決める)
}

void GamePlayScene::RequestResult(const std::string& sceneName, BlindTransition::StartState startState) {
	if (resultRequested_) {
		return; // 遷移は1度きり(SceneManager側も予約中は無視するが、進行停止はこのフラグで行う)
	}

	// エディタのPlayボタンから始めたプレイは、結果画面へ行かずEditへ戻る
	// (シーンを作り直すと未保存の配置・調整値・デバッグカメラの位置が消えるため)。
	// GAMEOVER・CLEAR・Force Clear・Force GameOverの4つの出口はすべてここを通るので、
	// この1箇所で分岐すれば全部に効く(被弾の入口をTakeDamageに集めたのと同じ考え方)。
	// ChangeSceneより前に分ける: 一度予約した遷移は、後からフラグを戻しても取り消せないため。
	// Updateの途中(レール終端・HP0)から呼ばれても安全:
	// - 敵はRetireEnemiesで退避されるだけで、破棄はEnemySpawner::Updateの冒頭(前フレームのGPU実行完了後)。
	//   このフレームの描画コマンドはまだ記録前なので、記録済みのコマンドを壊さない
	// - 以降の被弾・射撃・移動・GAMEOVER判定は、mode_==EditとPlayerへの許可(controllable)で止まる
	// - このフレームだけgameDeltaがPlayの値のまま残るが、自機は移動を止められ、
	//   弾はClear済み・敵は退避済みなので、動いて見えるものが無い
	if (startedFromEditor_) {
		EnterEdit();
		return;
	}

	resultRequested_ = true;
	// 結果画面へ表示用のスコアを預ける(ChangeSceneはシーン名しか渡せないため)
	ResultScene::SetScore(GetScore());
	// クリアならランク(駆除報告)も預ける。決め方(駆除率と被弾数)はStageProgressが持つ。
	// 女王を倒したフレームでも、区間の集計は敵の更新の直後に済んでいるので、最後の区間まで数えた結果になる
	if (sceneName == "CLEAR") {
		ResultScene::SetRank(stageProgress_->ComputeRank(GetDamageTaken()));
	}
	SceneManager::GetInstance()->ChangeScene(sceneName, std::make_unique<BlindTransition>(startState));
}

void GamePlayScene::Finalize() {
	// 結果画面へ鳴り続けないよう止める。
	// リトライで新しいGamePlaySceneが作られると、そちらのInitializeが鳴らし直す
	SoundManager::GetInstance()->StopWave(bgmHandle_);
	bgmHandle_ = 0;

	// パーティクルに渡したカメラはこのシーンと一緒に消えるので、参照を外しておく。
	// どちらのManagerもFrameworkが毎フレーム更新・描画するため、残すと結果画面で解放済みのカメラを読みに行く
	// (GPU側はエミッタが消えてもグループは残るので、Drawがカメラを読み続ける)。
	// 撃破の爆発の粒も消しておく: CPU側はカメラが無いと更新を止め、前の描画数のまま描くので、
	// 残すと結果画面に止まった爆発が映り、リトライで続きから動き出す(TitleSceneの炎の尾と同じ対処)
	ParticleManager::GetInstance()->ClearParticles(EnemySpawner::DefeatEffectName());
	// チャージ弾の尾と爆発も同じ理由で消す
	ParticleManager::GetInstance()->ClearParticles(kChargeTrailEffect);
	ParticleManager::GetInstance()->ClearParticles(kChargeExplosionEffect);
	// 巣の全滅の煙も同じ理由で消す
	ParticleManager::GetInstance()->ClearParticles(EnemySpawner::ExterminateEffectName());
	// ゲームオーバー演出の煙・火花・爆発の粒も同じ理由で消し、パーティクルの時間倍率を等速へ戻す
	// (倍率はParticleManagerの全グループに掛かるので、残すと次のシーンの粒までスローになる)
	gameOverSequence_->Stop();
	// クリア演出の炎・火花・煙も同じ(時間倍率も等速へ戻す)
	clearSequence_->Stop();
	ParticleManager::GetInstance()->SetCamera(nullptr);
	GPUParticleManager::GetInstance()->SetCamera(nullptr);
	// 光は全シーンで共有する状態なので、夜の光にしたこのシーンが元へ戻す
	// (戻さないと、次のシーン(タイトルなど)まで夜の光のまま描かれる。パーティクルの時間倍率と同じ決まり)
	Object3dCommon::GetInstance()->ResetLights();
}

void GamePlayScene::Update(float deltaTime) {
	// ヒットストップ: 残っている間はゲーム進行へ渡すdtを0にする。
	// タイマー自体は実dtで消化する(止めると永久に解除されない)。
	// カメラ・HUD・画面の揺れ、およびFrameworkが回すパーティクルは実dtのまま進むので、
	// 「自機と敵と弾だけが止まり、爆発は流れ続ける」形になる。
	// ゲームオーバー演出の間は、演出が決めた時間倍率(ヒットストップ=0 / スロー=0.25 / 等速=1)を掛ける。
	// 倍率は前のフレームの演出の更新で決まった値(1フレームの遅れは体感できない)。
	// クリア演出の倍率も同じく掛ける(2つの演出は同時には起きないので、演出していない方は1のまま)
	float gameDelta = deltaTime * gameOverSequence_->GetTimeScale() * clearSequence_->GetTimeScale();
	if (hitStopTimer_ > 0.0f) {
		hitStopTimer_ = (std::max)(hitStopTimer_ - deltaTime, 0.0f);
		gameDelta = 0.0f;
	}

	if (mode_ == Mode::Edit) {
		gameDelta = 0.0f; // 編集モード中はゲーム進行を止める(カメラ・HUD・パーティクルは動く)
	}

	// F1でデバッグカメラON/OFF(ImGuiのDebug Cameraウィンドウでも切替可)
	if (Input::GetInstance()->IsTriggerKey(DIK_F1)) {
		debugCamera_->SetActive(!debugCamera_->IsActive());
	}
#ifdef USE_IMGUI
	// Fキーで選択中オブジェクトへフォーカス(Sceneビュー上でのみ反応)
	if (debugCamera_->IsActive() && selectedIndex_ >= 0 &&
		Input::GetInstance()->IsSceneViewHovered() && Input::GetInstance()->IsTriggerKey(DIK_F)) {
		const EditorObject& target = editorObjects_[selectedIndex_];
		if (target.transform) {
			const Transform* transform = target.transform;
			float radius = (std::max)({ transform->scale.x, transform->scale.y, transform->scale.z });
			debugCamera_->FocusOn(transform->translate, radius);
		} else if (target.position) {
			// 位置だけの対象(レールの制御点)は大きさを持たないので、選択の判定球の大きさで寄る
			debugCamera_->FocusOn(*target.position, target.pickRadius > 0.0f ? target.pickRadius : 1.0f);
		}
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
	enemyBulletManager_->SetCamera(activeCamera);
	chargeBulletManager_->SetCamera(activeCamera);
	enemySpawner_->SetCamera(activeCamera);
	gameOverSequence_->SetCamera(activeCamera);
	clearSequence_->SetCamera(activeCamera);

	// ゲーム開始のイントロ。Enter/パッドSTARTでスキップする(終点はレールカメラと同じなので、止めるだけで定位置へ移る)。
	// タイトル・結果画面の「決定」に入っているSpace/パッドAを含めないのは、射撃と同じボタンのため
	// (含めると、スキップして操作できるようになった瞬間に1発撃ってしまう)。
	// 時間はヒットストップの影響を受けない実dtで進める
	if (introCamera_->IsPlaying() &&
		(Input::GetInstance()->IsTriggerKey(DIK_RETURN) || Input::GetInstance()->IsTriggerPad(XINPUT_GAMEPAD_START))) {
		introCamera_->Stop();
	}
	introCamera_->Update(deltaTime);
	// このフレームでイントロ中か。以降のレール前進・カメラ・Playerへの許可はすべてこの値で判断する
	const bool introPlaying = introCamera_->IsPlaying();

	// ゲームオーバー演出のスキップ(制御不能・遠ざかる の途中なら、その場で爆発へ進む)。
	// ボタンはイントロのスキップと同じEnter/パッドSTART(射撃のSpace/パッドAは含めない)
	if (gameOverSequence_->IsActive() &&
		(Input::GetInstance()->IsTriggerKey(DIK_RETURN) || Input::GetInstance()->IsTriggerPad(XINPUT_GAMEPAD_START))) {
		gameOverSequence_->Skip();
	}
	// このフレームでゲームオーバー演出中か(フレームの途中でHPが尽きて始まった場合は、次のフレームから効く)
	const bool gameOverActive = gameOverSequence_->IsActive();

	// クリア演出の段階を、スキップより前に控える。下で「このフレームで大爆発に入ったか」を見るとき、
	// スキップで大爆発へ飛んだ場合も変化として拾えるようにするため
	// (スキップの後に控えると、もう大爆発に入った後の値になり、揺れと全滅を取りこぼす)
	const ClearSequence::Phase clearPhaseBefore = clearSequence_->GetPhase();
	// クリア演出のスキップ(断末魔の途中なら大爆発へ、全滅・余韻の途中なら終わりへ)。ボタンはゲームオーバーと同じ
	if (clearSequence_->IsActive() &&
		(Input::GetInstance()->IsTriggerKey(DIK_RETURN) || Input::GetInstance()->IsTriggerPad(XINPUT_GAMEPAD_START))) {
		clearSequence_->Skip();
	}
	// このフレームでクリア演出中か(女王を倒したフレームに始まった場合は、次のフレームから効く)
	const bool clearActive = clearSequence_->IsActive();

	// レールカメラ: railDistance_はプレイヤーの進行度。カメラはプレイヤーの座標系を
	// 接線方向にcameraBackDistance_だけ下がった位置に置き、接線の向きで進行方向を向く。
	// rail(d-後方距離)を評価せずforwardで直接下がるのが肝(カメラとプレイヤーが同一の
	// レール座標系を共有し、カーブでも画面内の自機位置が数学的に固定される)。
	// camera_->Update()より前にTransformを書き、同フレームのview行列へ反映させる
	if (railCameraActive_) {
		const CatmullRomSpline& rail = stage_->GetRail();
		if (rail.GetTotalLength() > 0.0f) {
			// カメラのTransform更新は止めない: 止めると凍結中のフレームでビューが更新されなくなる。
			// イントロ中はレールを進めない(自機は始点に留まる=敵の出現タイミングはイントロの長さに左右されない)。
			// ゲームオーバー演出中も止める: カメラが止まった位置に残り、流されていく機体を見送る形になる
			// (新しい敵も出てこない)。クリア演出中も止める(女王の前で止まったまま、爆発を見届ける)
			if (!resultRequested_ && mode_ == Mode::Play && !introPlaying && !gameOverActive && !clearActive) {
				railDistance_ += railSpeed_ * gameDelta;

				if (railLoop_) {
					// 周回はデバッグ用(配置編集中に同じ区間を何度も確認する)
					const float previousDistance = railDistance_;
					railDistance_ = std::fmod(railDistance_, rail.GetTotalLength());
					// 一周して進行度が巻き戻ったら敵も未発生に戻す(周回するたび同じ敵が出る)
					if (railDistance_ < previousDistance) {
						enemySpawner_->Reset();
						bulletManager_->Clear(); // 前周の弾がレール先頭に取り残されないようにする
						enemyBulletManager_->Clear();
						chargeBulletManager_->Clear();
					}
				} else if (railDistance_ >= rail.GetTotalLength()) {
					// 終端を超えた分は切り捨てる(スプラインの評価範囲外へ出さない)。
					// 以降は毎フレームここで止まるので、終点でレールが止まったまま待つ形になる
					railDistance_ = rail.GetTotalLength();
					// 女王(倒すとクリアになる敵)がいるステージは、終点ではクリアにしない。
					// クリアは女王を倒したとき(敵の更新の後)に出す。いないステージは今までどおり終点到達でクリア
					if (!enemySpawner_->HasGoal()) {
						RequestResult("CLEAR");
					}
				}
			}

			Vector3 tangent = rail.GetTangentByDistance(railDistance_);
			Vector3 cameraPosition = rail.GetPositionByDistance(railDistance_) - tangent * cameraBackDistance_;

			// 接線→オイラー角。回転規約(行ベクトル×Rx→Ry→Rz)では
			// forward = (cos(pitch)·sin(yaw), -sin(pitch), cos(pitch)·cos(yaw)) なので逆算する
			float yaw = std::atan2(tangent.x, tangent.z);
			float pitch = std::atan2(-tangent.y, std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z));
			Vector3 cameraRotate = { pitch, yaw, 0.0f }; // バンク(roll)はしない

			// イントロ中は回り込みの軌道で上書きする。見つめるのはレール上の自機の位置
			// (イントロ中は移動させないのでオフセットは0のまま=レール上の点そのもの)。
			// 終点は上で求めたレールカメラの値と一致するので、イントロが終わった瞬間に画面が飛ばない
			if (introPlaying) {
				introCamera_->ComputePose(rail.GetPositionByDistance(railDistance_), tangent, cameraBackDistance_,
					cameraPosition, cameraRotate);
			}

			// ゲームオーバー演出中は、止まった位置のまま向きだけを墜落する機体へ向ける(見送り)
			if (gameOverActive) {
				gameOverSequence_->ComputeCameraRotate(cameraPosition, tangent, cameraRotate);
			}

			// 被弾時の画面の揺れ。レール上の基準位置を出した「後」にオフセットを足すだけなので、
			// レール座標系の計算(自機位置・狙点・敵の判定)には一切影響しない。
			// 減衰は実dtなので、ヒットストップで止まっている間も震え続ける
			if (shakeTimer_ > 0.0f) {
				shakeTimer_ = (std::max)(shakeTimer_ - deltaTime, 0.0f);
				const float amplitude = shakePower_ * (shakeTimer_ / shakeDuration_);
				// ワールド軸ではなくカメラのローカル右/上へずらす
				// (ワールド軸だとカーブの向きによって揺れ方が変わって見えるため)
				const Matrix4x4 rotation = Matrix4x4::Rotate(camera_->GetTransform().rotate);
				const Vector3 right = rotation.Transform({ 1.0f, 0.0f, 0.0f });
				const Vector3 up = rotation.Transform({ 0.0f, 1.0f, 0.0f });
				cameraPosition += right * Random::GetFloat(-amplitude, amplitude)
					+ up * Random::GetFloat(-amplitude, amplitude);
			}
			camera_->SetTranslate(cameraPosition);
			camera_->SetRotate(cameraRotate);
		}
	}

	camera_->Update();
	camera_->TransferToGPU();

	// プレイヤーより先に更新する: Stage::Updateがワールドコライダーを作り直すため、
	// この順なら同フレームの編集(ギズモ移動等)がそのまま当たり判定に反映される
	stage_->Update(gameDelta);

	// Edit中にレールの形が変わったら(敵の出現距離が付け直されたら)、出現地点の写しを持つEnemySpawnerを作り直す。
	// 前回作り直したときのレールの版と比べる「差分を見る」作法(Stageは誰が距離を写しているかを知らなくてよい)。
	// Play中は作り直さない: 通過済みの出現地点まで未発生に戻り、敵が一斉に出直してしまうため(次のPlayで反映される)
	if (mode_ == Mode::Edit && stage_->GetRailRevision() != spawnerRailRevision_) {
		enemySpawner_->BuildFromStage(stage_->GetData());
		spawnerRailRevision_ = stage_->GetRailRevision();
	}

	// 被弾の検出用に更新前のHPを控える。ダメージ源は地形(Player内部)と敵(この下)の2つに
	// 分かれているので、ダメージ源を問わず拾える「HPが減ったか」で判定する
	const int hpBeforeUpdate = player_->GetHp();

	// プレイヤーはレール上のrailDistance_地点そのもの(進行はレールカメラ側で行うため供給のみ)
	player_->SetRailDistance(railDistance_);
	// 被弾・射撃・移動の許可。モードとイントロをPlayerの言葉(被弾してよいか/撃ってよいか/動いてよいか)に翻訳して渡す。
	// 正しい値を持つのはmode_とイントロの状態だけにしたいので、切り替え時ではなく毎フレーム渡す
	// (初期モードがEnterPlay/EnterEditを通らなくても、Resetの呼び順に関係なく正しい値になる。
	// SetRailDistanceと同じ作法)
	// ゲームオーバー演出中も止める(機体はもう演出の墜落する機体に入れ替わっていて、自機は描かれていない)。
	// クリア演出中も止める(自機はその場に浮いたまま。決めポーズなどの動きはプレイヤーアクションの回で考える)
	const bool controllable = mode_ == Mode::Play && !introPlaying && !gameOverActive && !clearActive;
	player_->SetDamageEnabled(controllable);
	player_->SetShootEnabled(controllable);
	player_->SetMoveEnabled(controllable);
	player_->Update(gameDelta);

	// プレイヤーの直後に更新する: 同フレームに発射された弾もこのフレーム分だけ進むため、
	// 発射の1フレーム目が銃口に張り付いて見えることがない(チャージ弾も同じ理由でここ)
	bulletManager_->Update(gameDelta);
	chargeBulletManager_->Update(gameDelta);

	// 進行度がSpawnPointのrailDistanceを超えたら敵が発生する。
	// 弾の更新後に呼ぶことで、弾が今フレーム分進んだ位置で被弾判定できる
	// 進行方向はレールの接線を渡す。射撃型の敵が自機を追い越して背後から撃つのを防ぐために使う
	// (「自機→狙点」の向きは、Aim Convergenceを1より大きくすると照準に合わせて横へ大きく傾き、
	// 進行方向の代わりにならない)
	const int explosionCountBefore = enemySpawner_->GetExplosionCount();
	enemySpawner_->Update(railDistance_, gameDelta, player_->GetWorldPosition(),
		player_->GetCollisionRadius(), player_->GetForward());

	// チャージ弾が敵に当たって爆発したら、小さめの揺れと短いヒットストップで手応えを出す。
	// 被弾(下のHPの比較)と同じく「更新の前後の差」で拾うので、EnemySpawnerはシーンの演出を知らなくてよい
	if (enemySpawner_->GetExplosionCount() > explosionCountBefore) {
		StartShake(explosionShakeStrength_);
		hitStopTimer_ = (std::max)(hitStopTimer_, explosionHitStopDuration_);
	}

	// 区間ごとの集計。敵の更新(撃破・取り逃しの記録)の後に数え直す。
	// 区切りはEditで編集中の値をそのまま使う(Saveしなくても集計に効く)
	stageProgress_->Update(stage_->GetData(), stage_->GetRail().GetTotalLength(), *enemySpawner_);
	// 今フレーム確定した区間があれば結果を出す。同じフレームに2つ確定したら後の区間を見せる
	// (Showは表示中でも差し替えるので、ループの最後の1つが残る)
	for (size_t sectionIndex : stageProgress_->GetNewlyResolved()) {
		sectionBanner_->Show(stageProgress_->GetSections()[sectionIndex]);
	}

	// 女王を倒したらクリア(巣の駆除が完了)。終点に着く前に倒した場合も同じくクリアにする。
	// 撃破の判定は敵の更新の中で済んでいるので、ここはその結果を見てクリア演出を始めるだけ
	// (HPが尽きたときのゲームオーバーと同じく、勝敗の判断はシーンが持つ)。
	// 結果画面へは、演出が終わってから進む(下)
	if (!resultRequested_ && mode_ == Mode::Play && !gameOverSequence_->IsActive() &&
		!clearSequence_->IsActive() && enemySpawner_->IsGoalDefeated()) {
		StartClear();
	}

	// ロックオン。敵の更新の後に行う: 撃破された敵のロックを外し、今フレームの敵の位置で重なりを見るため。
	// 結果をPlayerが使うのは次フレームの射撃(離した瞬間)になるが、1フレーム(約16ms)の遅れは体感できない
	UpdateLockOn();

	// 敵弾の更新は敵の更新後。こうすると、このフレームに発射された弾もその場で1フレーム分進み、
	// 発射の瞬間だけ敵に張り付いて見えることがない(自弾とPlayerの順番と同じ考え方)
	enemyBulletManager_->Update(gameDelta);

	// 敵との接触ダメージ。接触の判定は敵側、無敵時間の管理はPlayer側にあるので、
	// シーンは「触れていたら殴る」という繋ぎ込みだけを持つ。
	// 地形との接触はPlayer::Updateが自分で適用している
	// 触れていた敵の位置を「どこから当たったか」として渡す(ゲームオーバー演出の煙の出所になる)
	if (!resultRequested_ && enemySpawner_->IsPlayerHit() && mode_ == Mode::Play) {
		player_->TakeDamage(1, enemySpawner_->GetPlayerHitPosition());
	}

	// 今フレームHPが減っていたらヒットストップと画面の揺れを起こす。
	// 地形・敵のどちらで減ったかを区別せず、Playerの無敵時間で弾かれた分も自動的に除外される
	if (player_->GetHp() < hpBeforeUpdate) {
		hitStopTimer_ = hitStopDuration_;
		StartShake(shakeStrength_);
		damageEffectTimer_ = damageEffectDuration_;
	}

	// HPが尽きたらゲームオーバー演出を始める(A-5)。結果画面へは、演出が真っ黒になり切ってから進む(下)。
	// (クリアの判定は、上の女王の撃破と、女王がいないステージのレール終端にある)。
	// クリア演出の間は始めない: 演出中は被弾を止めているので本来HPは減らないが、2つの演出が重なると
	// 時間倍率とポストエフェクトを奪い合うので、念のため入口でも分けておく
	if (!resultRequested_ && player_->IsDead() && mode_ == Mode::Play && !gameOverSequence_->IsActive() &&
		!clearSequence_->IsActive()) {
		StartGameOver();
	}

	// HUDの色と頂点を更新(HPが確定した後)。フェードインはヒットストップで止めないので実dt
	// ドローンのライト。自機と照準の更新(移動・照準の吸い付き)が済んだ後に、今フレームの位置と向きで照らす
	UpdateHeadlight();

	UpdateHud(deltaTime);

	// ゲームオーバー演出。段階のタイマー・機体・パーティクル・ポストエフェクトは実dtを渡して演出に任せる
	// (スローの倍率は演出が自分で掛け、ゲーム側へはGetTimeScaleで次のフレームから効く)。
	// 爆発の揺れは「更新の前後で段階が爆発に変わったか」で拾う: チャージ弾の爆発と同じく、
	// 演出はシーンの画面の揺れを知らなくてよい(Skipで飛んだ場合も同じ判定で拾える)
	const GameOverSequence::Phase gameOverPhaseBefore = gameOverSequence_->GetPhase();
	gameOverSequence_->Update(deltaTime, *GetActiveCamera());
	if (gameOverPhaseBefore != GameOverSequence::Phase::Explode &&
		gameOverSequence_->GetPhase() == GameOverSequence::Phase::Explode) {
		StartShake(gameOverSequence_->GetParams().explosionShake);
	}
	// 真っ黒になり切ったら結果画面へ。画面はもう黒いので、遷移の帯は閉じた状態から始めてすぐ切り替える
	// (タイトル→本編の遷移と同じ)。エディタのPlayから始めたプレイなら、RequestResultがEditへ戻す
	if (gameOverSequence_->IsFinished()) {
		RequestResult("GAMEOVER", BlindTransition::StartState::Closed);
	}

	// クリア演出。ゲームオーバー演出と同じく、段階のタイマー・亡骸・パーティクル・ポストエフェクトは実dtで演出に任せる。
	// 画面の揺れと残りの敵の全滅は「更新の前後の差」で拾う(演出はシーンの揺れもEnemySpawnerも知らなくてよい)
	const int clearPopCountBefore = clearSequence_->GetPopCount();
	clearSequence_->Update(deltaTime, *GetActiveCamera());
	if (clearSequence_->GetPopCount() > clearPopCountBefore) {
		StartShake(clearSequence_->GetParams().popShake); // 小爆発のたびに小さく揺らす
	}
	// 大爆発に入ったフレーム(スキップで飛んだ場合も含む。控えはスキップより前)
	if (clearPhaseBefore != ClearSequence::Phase::Burst && clearSequence_->GetPhase() == ClearSequence::Phase::Burst) {
		StartShake(clearSequence_->GetParams().burstShake);
		// 残りの敵を全滅させる(色を失って落ち、順に消える)。Edit中のプレビューでは本物の敵に触らない
		if (mode_ == Mode::Play) {
			enemySpawner_->ExterminateAll();
		}
	}
	// 演出が終わったら結果画面へ。画面は明るいままなので、遷移の帯は開いた状態から閉じていく(従来のクリアと同じ)。
	// エディタのPlayから始めたプレイなら、RequestResultがEditへ戻す。
	// Edit中のプレビュー(Preview Clear)なら、遷移せずに演出を止めるだけ
	if (clearSequence_->IsFinished()) {
		if (mode_ == Mode::Edit) {
			clearSequence_->Stop();
		} else {
			RequestResult("CLEAR");
		}
	}

	// 被弾の画面着色。ヒットストップで止めたくないので実dt(deltaTime)を渡す
	UpdateDamageEffect(deltaTime);

	if (showRail_) {
		DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 10.0f, 20, { 1.0f,1.0f,1.0f,0.5f });
	}

	// レール曲線の可視化(stage.json手編集→Reloadの確認用)。曲線=赤の折れ線、制御点=黄の球。
	// railは下のSpawnPoint表示でも使うので、参照の取得はチェックの外で行う
	const CatmullRomSpline& rail = stage_->GetRail();
	const std::vector<Vector3>& railPoints = rail.GetControlPoints();
	// 制御点を選んでいる間は、Show RailがOFFでもレールを描く(動かしているものが見えないと編集できないため)
	const int selectedRailPoint = SelectedRailPointIndex();
	const bool railVisible = showRail_ || selectedRailPoint >= 0;
	if (railVisible && railPoints.size() >= 2) {
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
	if (railVisible) {
		for (size_t i = 0; i < railPoints.size(); ++i) {
			// 選んでいる制御点は白く大きく描き、どれを動かしているかを一目で分かるようにする
			const bool selected = static_cast<int>(i) == selectedRailPoint;
			DebugRenderer::GetInstance()->AddSphere(railPoints[i], selected ? 1.0f : 0.5f,
				selected ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 0.2f, 1.0f });
		}
		// 区間の区切り(各区間の始まり)。レール上の点に球を置き、真上へ線を伸ばす。
		// "Sections"ウィンドウで開始距離を動かすと、この目印がレールに沿って動く
		if (railPoints.size() >= 2) {
			for (const StageData::SectionData& section : stage_->GetData().sections) {
				const Vector3 position = rail.GetPositionByDistance(section.startDistance);
				DebugRenderer::GetInstance()->AddSphere(position, 1.5f, kSectionMarkerColor);
				DebugRenderer::GetInstance()->AddLine(
					position, position + Vector3{ 0.0f, kSectionMarkerHeight, 0.0f }, kSectionMarkerColor);
			}
		}
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
		// 敵の判定の球も出す(女王のように判定の中心を前へずらした敵で、見た目と判定が合っているかを確かめる)
		enemySpawner_->DrawHitSpheres();
	}

	// 照準の可視化(レティクル(A-2)が入るまでの確認用)。
	// 銃口→狙点の線と狙点の球を出すと、Aim Convergenceを変えたときに弾道が
	// レール中心へ寄っていく様子がそのまま目で追える
	if (showAimLine_) {
		const Vector4 aimColor = { 0.4f, 1.0f, 1.0f, 1.0f };
		DebugRenderer::GetInstance()->AddLine(
			player_->GetMuzzlePosition(), player_->GetAimPoint(), aimColor);
		DebugRenderer::GetInstance()->AddSphere(player_->GetAimPoint(), 0.5f, aimColor);

		// ロック中は、銃口からロックした敵まで線を引く(ロックオンが効いていることの確認用)
		Vector3 lockPosition{};
		if (enemySpawner_->TryGetEnemyPosition(player_->GetLockTargetId(), lockPosition)) {
			const Vector4 lockColor = { 1.0f, 0.35f, 0.2f, 1.0f };
			DebugRenderer::GetInstance()->AddLine(player_->GetMuzzlePosition(), lockPosition, lockColor);
			DebugRenderer::GetInstance()->AddSphere(lockPosition, 1.0f, lockColor);
		}
	}

	// SpawnPoint(敵の発生地点)の可視化。未発生=黄、発生済み=グレー。
	// 「レール上のどの進行度で出るか」が分かるよう、発生地点とレール上の該当点を線で結ぶ
	// 点光源の可視化。位置に光の色の小さな球、届く範囲に薄い大きな球を描く。
	// ドローンのライトは銃口から照らす向きへ、届く距離の分だけ線を引く
	if (showLights_) {
		for (const StageData::ObjectData& objectData : stage_->GetData().objects) {
			if (objectData.type != StageData::ObjectType::Light || objectData.disabled) {
				continue;
			}
			const Vector3& color = objectData.light.color;
			const Vector3& position = objectData.transform.translate;
			DebugRenderer::GetInstance()->AddSphere(position, 3.0f, { color.x, color.y, color.z, 1.0f });
			DebugRenderer::GetInstance()->AddSphere(position, objectData.light.radius, { color.x, color.y, color.z, 0.25f });
		}
		const StageData::LightingData& lighting = stage_->GetData().lighting;
		const Vector3 toAim = player_->GetAimPoint() - player_->GetMuzzlePosition();
		if (stage_->GetData().hasLighting && toAim.LengthSquared() > 1.0e-6f) {
			DebugRenderer::GetInstance()->AddLine(player_->GetMuzzlePosition(),
				player_->GetMuzzlePosition() + Vector3::Normalized(toAim) * lighting.headlightDistance,
				{ 1.0f, 1.0f, 0.8f, 1.0f });
		}
	}

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

	stage_->Draw();
	// ゲームオーバー演出中は、自機の代わりに演出の墜落する機体を描く(同じモデル・同じ姿勢から始まるので入れ替わりは見えない)
	if (gameOverSequence_->IsActive()) {
		gameOverSequence_->Draw();
	} else {
		player_->Draw();
	}
	bulletManager_->Draw();
	chargeBulletManager_->Draw();
	enemyBulletManager_->Draw();
	enemySpawner_->Draw();
	// クリア演出中は女王の亡骸を描く(女王本体は撃破でEnemySpawnerから消えているので、入れ替わりは見えない)
	clearSequence_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*GetActiveCamera());

	// HUDは3Dの最後(HPと操作説明)
	DrawHud();
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	// Hierarchy: オブジェクト一覧+選択、シーンファイル操作
	ImGui::Begin("Hierarchy");

	// Play in Editorの操作。エディタで一番使うボタンなので、スクロールしなくても届く先頭に置く
	ImGui::BeginDisabled(mode_ == Mode::Play);
	if (ImGui::Button("Play")) {
		EnterPlay();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(mode_ == Mode::Edit);
	if (ImGui::Button("Stop")) {
		EnterEdit();
	}
	ImGui::SameLine();
	// 開始時の状態からイントロでやり直す(イントロの見直しも兼ねる)。
	// Edit中に押すとPlayと同じ動きになって重複するので、Play中だけ押せるようにする
	if (ImGui::Button("Restart")) {
		EnterPlay();
	}
	ImGui::EndDisabled();
	// 今のモードを色付きで出す(ボタンの有効/無効だけだと、どちらのモードか一目で分かりにくいため)
	ImGui::SameLine();
	if (mode_ == Mode::Play) {
		ImGui::TextColored({ 0.3f, 1.0f, 0.4f, 1.0f }, "PLAYING");
	} else {
		ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "EDIT");
	}
	ImGui::Separator();

	EditorPanels::DrawHierarchy(editorObjects_, selectedIndex_);

	// ステージオブジェクトの追加/複製/削除(stage.jsonの管轄分のみ。C++直書きの固定オブジェクトは対象外)
	ImGui::SeparatorText("Stage Objects");
	// 配置オブジェクトは[固定]と[制御点]の間。制御点を選んでいるときにDuplicate/Deleteが配置オブジェクトに効かないよう、後ろも区切る
	const bool stageObjectSelected = selectedIndex_ >= static_cast<int>(fixedEditorObjectCount_) &&
		selectedIndex_ < static_cast<int>(railEditorObjectStart_);
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
	// 点光源を追加する。これもstatic・spawnと同じobjects[]に入るので、Hierarchy・ギズモ・複製・削除・無効化がそのまま使える。
	// 光への反映はStage::Updateが毎フレーム行うので、ここでは追加するだけ
	if (ImGui::Button("Add Light")) {
		StageData::ObjectData objectData;
		objectData.type = StageData::ObjectType::Light;
		objectData.name = "light";
		const Transform& cameraTransform = GetActiveCamera()->GetTransform();
		Vector3 forward = Matrix4x4::Rotate(cameraTransform.rotate).Transform({ 0.0f, 0.0f, 1.0f });
		objectData.transform.translate = cameraTransform.translate + forward * 10.0f;

		size_t newIndex = stage_->AddObject(std::move(objectData));
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

	// 格子状に複製(床の板のように同じ物を一定間隔で敷き詰める)。選んだ静的オブジェクトを(-X,-Z)側の角にして並べる。
	// Inspectorではなくここに置くのは、複製後に一覧を作り直す(RebuildEditorObjects)と、
	// 実行中のInspectorの描画関数(drawInspector)ごと消えてしまうため
	const bool staticSelected = stageObjectSelected &&
		stage_->GetData().objects[stageIndex].type == StageData::ObjectType::Static;
	if (ImGui::TreeNode("Grid Duplicate")) {
		ImGui::DragInt("Count X", &gridCountX_, 0.1f, 1, 20);
		ImGui::DragInt("Count Z", &gridCountZ_, 0.1f, 1, 20);
		ImGui::DragFloat("Spacing X", &gridSpacingX_, 1.0f, 1.0f, 5000.0f, "%.0f");
		ImGui::DragFloat("Spacing Z", &gridSpacingZ_, 1.0f, 1.0f, 5000.0f, "%.0f");
		ImGui::BeginDisabled(!staticSelected);
		if (ImGui::Button("Duplicate Grid")) {
			stage_->DuplicateObjectGrid(stageIndex, gridCountX_, gridCountZ_, gridSpacingX_, gridSpacingZ_);
			RebuildEditorObjects(); // 要素数が変わりTransformポインタが無効になるため必須
			selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + stageIndex); // 元のオブジェクトを選んだままにする
		}
		ImGui::EndDisabled();
		if (!staticSelected) {
			ImGui::TextDisabled("Select a static object (e.g. floor_board).");
		}
		ImGui::TreePop();
	}

	// レールの制御点の追加/削除(Stage Objectsと同じく、選んでいる対象に効く)。
	// 制御点の移動はギズモ/Inspectorで行い、Stage::Updateがその場でレールを作り直す。
	// どの操作でも、区間の開始距離と敵の出現距離はレール上の同じ場所に付いてくる(Stage::RebuildRailKeepingDistances)
	ImGui::SeparatorText("Rail");
	const std::vector<Vector3>& railPoints = stage_->GetData().rail;
	ImGui::Text("Points: %zu  Length: %.1f", railPoints.size(), stage_->GetRail().GetTotalLength());
	const int selectedRailPoint = SelectedRailPointIndex();
	ImGui::BeginDisabled(selectedRailPoint < 0);
	if (ImGui::Button("Insert Point After")) {
		const size_t newIndex = stage_->InsertRailPoint(static_cast<size_t>(selectedRailPoint));
		RebuildEditorObjects(); // 制御点の数が変わりpositionポインタが無効になるため必須
		selectedIndex_ = static_cast<int>(railEditorObjectStart_ + newIndex); // 足した点をそのまま動かせるように選ぶ
	}
	ImGui::SameLine();
	// 曲線は制御点2個以上で作れるので、それ以下にはしない(Stage::RemoveRailPointも同じ判定で守っている)
	ImGui::BeginDisabled(railPoints.size() <= 2);
	if (ImGui::Button("Delete Point")) {
		stage_->RemoveRailPoint(static_cast<size_t>(selectedRailPoint));
		RebuildEditorObjects();
		// 続けて消したり動かしたりできるよう、1つ手前の点を選ぶ(先頭を消したら新しい先頭)
		selectedIndex_ = static_cast<int>(railEditorObjectStart_) + (std::max)(selectedRailPoint - 1, 0);
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	if (selectedRailPoint < 0) {
		ImGui::TextDisabled("Select a rail point (rail_XX) to insert or delete.");
	}

	// ステージ配置のファイル操作。Reloadはstage.jsonの内容で上書き(未保存の編集は破棄される)
	ImGui::SeparatorText("Stage File");
	if (ImGui::Button("Save##stage")) {
		// Capture: カメラ調整値のライブ値をStageDataへ書き戻してから保存する
		// (Save=Capture+書き出し。所有者はシーンなので、Camera APIを呼ぶのはここ)
		StageData& data = stage_->GetData();
		data.hasCamera = true;
		data.camera = { camera_->GetFovY(), railSpeed_, cameraBackDistance_,
			camera_->GetNearClip(), camera_->GetFarClip() };
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
	// ONにすると終端で先頭へ戻る(クリア遷移せずに配置を何度も見たいとき用)
	ImGui::Checkbox("Loop (debug)", &railLoop_);
	ImGui::DragFloat("Speed", &railSpeed_, 0.1f, 0.0f, 100.0f, "%.1f m/s");
	ImGui::DragFloat("Camera Back", &cameraBackDistance_, 0.1f, 0.0f, 50.0f, "%.1f m");
	ImGui::Text("Distance: %.1f / %.1f m", railDistance_, stage_->GetRail().GetTotalLength());

	// 勝敗まわり(A-5)。遷移待ちの間はゲーム進行が止まっていることをここで確認できる
	ImGui::SeparatorText("Game");
	ImGui::Text("HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
	ImGui::Text("Result: %s", resultRequested_ ? "requested (frozen)" : "playing");
	// クリア条件。女王(Goalの種別)がステージにいれば「倒すとクリア」、いなければ「終点到達でクリア」
	if (!enemySpawner_->HasGoal()) {
		ImGui::Text("Goal: none (clear at rail end)");
	} else {
		ImGui::Text("Goal: %s", enemySpawner_->IsGoalDefeated() ? "defeated" : "alive (rail stops at end)");
	}
	ImGui::Checkbox("Show HUD", &showHud_);

	// 被弾演出の調整。ヒットストップは長すぎると操作が引っかかるので0.05〜0.15が目安
	ImGui::SeparatorText("Damage Feedback");
	ImGui::DragFloat("Hit Stop", &hitStopDuration_, 0.005f, 0.0f, 0.5f, "%.3f s");
	ImGui::DragFloat("Shake Time", &shakeDuration_, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::DragFloat("Shake Power", &shakeStrength_, 0.05f, 0.0f, 5.0f, "%.2f m");
	ImGui::DragFloat("Flash Time", &damageEffectDuration_, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::SliderFloat("Flash Power", &damageVignettePower_, 0.0f, 1.0f, "%.2f");
	if (ImGui::Button("Test Damage")) {
		hitStopTimer_ = hitStopDuration_;
		StartShake(shakeStrength_);
		damageEffectTimer_ = damageEffectDuration_;
	}

	// チャージ弾の爆発の手応え。被弾より弱くして、被弾の揺れと区別できるようにする
	ImGui::SeparatorText("Explosion Feedback");
	ImGui::DragFloat("Explosion Hit Stop", &explosionHitStopDuration_, 0.005f, 0.0f, 0.5f, "%.3f s");
	ImGui::DragFloat("Explosion Shake", &explosionShakeStrength_, 0.05f, 0.0f, 5.0f, "%.2f m");
	// Edit中は押せなくする。Edit中は進行中のプレイが無いので、プレイの出口を試す意味がない。
	// また、タイトルから始めてStopでEditに入った場合はstartedFromEditor_がfalseなので、
	// 押すと結果画面へ遷移してしまう
	// ゲームオーバー演出・クリア演出の間も押せなくする(プレイの出口へもう向かっている)
	ImGui::BeginDisabled(mode_ == Mode::Edit || gameOverSequence_->IsActive() || clearSequence_->IsActive());
	if (ImGui::Button("Force Clear")) {
		// 女王が出ていれば倒したことにして、本番と同じく演出から通す(撃破は次のUpdateで普段どおり処理され、
		// そこでクリア演出が始まる)。まだ出ていなければ亡骸を置く場所が無いので、今までどおり結果画面へ直行する
		if (!enemySpawner_->ForceDefeatGoal()) {
			RequestResult("CLEAR");
		}
	}
	ImGui::SameLine();
	// 本番(HPが尽きたとき)と同じく演出から通す。演出の調整値は"Game Over"ウィンドウ
	if (ImGui::Button("Force GameOver")) {
		StartGameOver();
	}
	ImGui::EndDisabled();
	// クリア演出だけを、女王の出現地点で再生する(Edit中のみ)。女王までプレイしなくても見た目を調整できる。
	// 本物の敵・弾・集計には触らず、終わったら止めるだけ(結果画面へは行かない)。調整値は"Clear"ウィンドウ
	ImGui::BeginDisabled(mode_ != Mode::Edit || clearSequence_->IsActive());
	if (ImGui::Button("Preview Clear")) {
		EnemySpawner::GoalPose pose;
		if (enemySpawner_->TryGetGoalSpawnPose(pose)) {
			clearSequence_->Start(EnemySpawner::ModelPathOf(pose.enemy), pose.transform, pose.center, pose.radius);
			StartShake(clearSequence_->GetParams().impactShake);
		}
	}
	ImGui::EndDisabled();

	// 当たり判定の可視化切替(コライダーのワイヤーボックス+プレイヤーの判定球)
	ImGui::SeparatorText("Collision");
	ImGui::Checkbox("Show Colliders", &showColliders_);
	ImGui::Checkbox("Show Rail", &showRail_);
	ImGui::Checkbox("Show Lights", &showLights_);
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

	// イントロの進み具合と軌道の調整値(見直しはHierarchy先頭のRestartボタン)
	introCamera_->DrawImGui();

	// ゲームオーバー演出の進み具合と調整値(再生はHierarchyのForce GameOver)
	gameOverSequence_->DrawImGui();

	// クリア演出の進み具合と調整値(再生はHierarchyのForce Clear / Preview Clear)
	clearSequence_->DrawImGui();

	// 弾の生存数・速度・寿命・判定半径
	bulletManager_->DrawImGui();

	// 敵弾・チャージ弾も同じUI(ウィンドウ名だけProfileで分けてある)
	enemyBulletManager_->DrawImGui();
	chargeBulletManager_->DrawImGui();

	// 敵の生存数・撃破数・HP・移動のパラメータ
	enemySpawner_->DrawImGui();

	// 区間(ステージの区切り)の編集
	DrawSectionsWindow();

	// ステージ全体の光(環境光・月明かり・ドローンのライト)の編集
	DrawLightingWindow();

	// 区間ごとの集計・今クリアしたときのランク・ランクの基準
	stageProgress_->DrawImGui(GetDamageTaken());

	// Sceneビューのクリックで選択を更新し、選択中の対象にギズモを表示
	// (デバッグカメラON中はその視点で描画されているため、ピック・ギズモも同じカメラで行う)
	Camera* activeCamera = GetActiveCamera();
	// 制御点は、レールを表示しているときだけクリックで選べる(見えないものは選べない。無効なオブジェクトと同じ決まり)。
	// Hierarchyからはいつでも選べ、選んでいる間はShow RailがOFFでもレールを描く(Update)
	for (size_t i = railEditorObjectStart_; i < editorObjects_.size(); ++i) {
		editorObjects_[i].pickable = showRail_;
	}
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
	// Cameraは実体が見えないためクリック選択の対象外
	editorObjects_.clear();
	editorObjects_.push_back({ "Camera", &camera_->GetTransform(), false });
	editorObjects_.back().scaleEditable = false; // カメラのscaleはビュー行列を歪ませるため編集させない

#ifdef USE_IMGUI
	// 型別のInspector追加UI(ImGui呼び出しを含むためDebug構成のみ)
	editorObjects_.back().drawInspector = [this]() {
		float fovY = camera_->GetFovY();
		if (ImGui::DragFloat("FovY", &fovY, 0.01f)) {
			camera_->SetFovY(fovY);
		}
		// 描画する奥行きの範囲(1単位=1cm)。手前を小さくするほど遠くの面がちらつきやすくなる。
		// 手前<奥を崩さないよう、互いの値を範囲の端にする(AlwaysClampで直接入力でも越えさせない)
		float nearClip = camera_->GetNearClip();
		float farClip = camera_->GetFarClip();
		bool clipChanged = ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.01f, farClip - 1.0f, "%.2f",
			ImGuiSliderFlags_AlwaysClamp);
		clipChanged |= ImGui::DragFloat("Far Clip", &farClip, 10.0f, nearClip + 1.0f, 100000.0f, "%.0f",
			ImGuiSliderFlags_AlwaysClamp);
		if (clipChanged) {
			SetCameraClip(nearClip, farClip);
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

			if (data.type == StageData::ObjectType::Light) {
				// 点光源: 位置はtransform(ギズモ)で置き、ここでは光の性質を編集する。
				// StageDataを直接書き換え、Stage::Updateが毎フレーム光へ反映するので、動かしながら照らし方を確かめられる
				ImGui::TextUnformatted("Type: Light (point light)");
				ImGui::ColorEdit3("Color", &data.light.color.x);
				ImGui::DragFloat("Intensity", &data.light.intensity, 0.01f, 0.0f, 10.0f, "%.2f");
				ImGui::DragFloat("Radius", &data.light.radius, 1.0f, 1.0f, 5000.0f, "%.0f");
				ImGui::DragFloat("Decay", &data.light.decay, 0.01f, 0.01f, 10.0f, "%.2f");
				if (!stage_->GetData().hasLighting) {
					// lighting項目が無いと光そのものを適用しない(昼の見た目)ので、気付けるように出す
					ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "Stage lighting is OFF (Lighting window).");
				}
				return; // 点光源にモデル選択・コライダーは不要
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

	// レールの制御点を最後に連結する(positionはStageData直指し。動かすとStage::Updateがレールを作り直す)。
	// 配置オブジェクトの後ろに置くので、上のループ(モデル差し替え・無効フラグ)の対象には入らない
	railEditorObjectStart_ = editorObjects_.size();
	std::vector<EditorObject> railObjects = stage_->BuildRailEditorObjects();
	editorObjects_.insert(editorObjects_.end(),
		std::make_move_iterator(railObjects.begin()), std::make_move_iterator(railObjects.end()));

#ifdef USE_IMGUI
	// 制御点のInspector追加UI: 先頭からの距離。区間の区切りや敵の出現距離を決めるときの目安にする
	for (size_t i = railEditorObjectStart_; i < editorObjects_.size(); ++i) {
		const size_t railPointIndex = i - railEditorObjectStart_;
		editorObjects_[i].drawInspector = [this, railPointIndex]() {
			const CatmullRomSpline& rail = stage_->GetRail();
			ImGui::Text("Rail Point %zu / %zu", railPointIndex + 1, stage_->GetData().rail.size());
			// 制御点iは区間iの始まり(最後の点だけは終点)
			const float distance = railPointIndex < rail.GetSegmentCount()
				? rail.GetSegmentStartDistance(railPointIndex)
				: rail.GetTotalLength();
			ImGui::Text("Distance from start: %.1f", distance);
		};
	}
#endif

	// 一覧が変わったので選択は解除(古いindexは別物を指しうる)
	selectedIndex_ = -1;
}

int GamePlayScene::SelectedRailPointIndex() const {
	if (selectedIndex_ < static_cast<int>(railEditorObjectStart_) ||
		selectedIndex_ >= static_cast<int>(editorObjects_.size())) {
		return -1;
	}
	return selectedIndex_ - static_cast<int>(railEditorObjectStart_);
}

void GamePlayScene::ApplyCameraFromStage() {
	const StageData& data = stage_->GetData();
	if (!data.hasCamera) {
		return; // camera項目の無い旧stage.json。ライブ値をデフォルトへ巻き戻さない
	}
	camera_->SetFovY(data.camera.fovY);
	railSpeed_ = data.camera.railSpeed;
	cameraBackDistance_ = data.camera.backDistance;
	SetCameraClip(data.camera.nearClip, data.camera.farClip);
}

void GamePlayScene::UpdateHeadlight() {
	// ステージの光が無いときは、Stage::ApplyLightsが光ごと昼の初期値へ戻している(スポットライトも消灯)ので何もしない
	const StageData& data = stage_->GetData();
	if (!data.hasLighting) {
		return;
	}
	const StageData::LightingData& lighting = data.lighting;

	SpotLight spotLight{};
	spotLight.color = { lighting.headlightColor.x, lighting.headlightColor.y, lighting.headlightColor.z, 1.0f };
	// ゲームオーバー演出の間は消す(機体が壊れて制御を失った)。照らし続けると、墜落の暗転に明るい円が残る
	spotLight.intensity = gameOverSequence_->IsActive() ? 0.0f : lighting.headlightIntensity;
	spotLight.distance = lighting.headlightDistance;
	spotLight.decay = lighting.headlightDecay;
	// 縁のぼかしは「この角度から外側へ暗くなる」。外枠の角度以上にするとシェーダの割り算が0除算になるので、内側に収める
	const float falloffStart = (std::min)(lighting.headlightFalloffStart, lighting.headlightAngle * 0.99f);
	spotLight.cosAngle = std::cos(lighting.headlightAngle);
	spotLight.cosFalloffStart = std::cos(falloffStart);

	// 銃口から照準の先へ向ける。照らされている所が「これから撃つ所」になり、ライトが狙いの助けになる
	spotLight.position = player_->GetMuzzlePosition();
	const Vector3 toAim = player_->GetAimPoint() - spotLight.position;
	spotLight.direction = toAim.LengthSquared() > 1.0e-6f
		? Vector3::Normalized(toAim)
		: player_->GetForward();
	Object3dCommon::GetInstance()->SetSpotLight(spotLight);
}

void GamePlayScene::SetCameraClip(float nearClip, float farClip) {
	camera_->SetNearClip(nearClip);
	camera_->SetFarClip(farClip);
	debugCamera_->GetCamera()->SetNearClip(nearClip);
	debugCamera_->GetCamera()->SetFarClip(farClip);
}

void GamePlayScene::ResetGameState() {
	railDistance_ = 0.0f;
	player_->Reset();
	enemySpawner_->Reset(); // 敵も未発生に戻す
	bulletManager_->Clear(); // 飛行中の弾も消す
	enemyBulletManager_->Clear();
	chargeBulletManager_->Clear();

	// シーンが持つ状態
	resultRequested_ = false;
	hitStopTimer_ = 0.0f;
	shakeTimer_ = 0.0f;
	damageEffectTimer_ = 0.0f;
	// 区間の結果表示は消す(集計そのものは敵の記録から数え直すので、戻す必要はない)
	sectionBanner_->Hide();
	// イントロは止めるだけ(始めるのはInitializeとEnterPlay)。
	// ここで始めると、Stopボタン(Editへ入る)でも再生されてしまう
	introCamera_->Stop();
	// ゲームオーバー演出も止める(墜落する機体・粒・時間倍率・ポストエフェクトを元に戻す)
	gameOverSequence_->Stop();
	// クリア演出も止める(亡骸・粒・時間倍率・ポストエフェクトを元に戻す。固まった敵はenemySpawner_->Resetが戻す)
	clearSequence_->Stop();
	// 演出でBGMをフェードアウトしていたら鳴らし直す。フェードし切ると再生が止まり、
	// フェードの途中ならこのまま消えていくので、どちらの場合も止めてから最初から鳴らす
	// (止まっている再生をStopWaveしても何も起きない)
	if (bgmFadedOut_) {
		SoundManager::GetInstance()->StopWave(bgmHandle_);
		bgmHandle_ = SoundManager::GetInstance()->PlayWave(bgmSound_, true, SoundManager::SoundCategory::BGM);
		bgmFadedOut_ = false;
	}
}

void GamePlayScene::EnterPlay() {
	ResetGameState();
	// 出現地点の写しをステージデータから作り直す(Play = 取り込み + 作り直し)。
	// EnemySpawnerは出現地点を写して持つので、Edit中にギズモで動かした位置や、
	// レールの編集で付け直した出現距離は、ここで写し直さないとゲームに届かない
	enemySpawner_->BuildFromStage(stage_->GetData());
	spawnerRailRevision_ = stage_->GetRailRevision();
	mode_ = Mode::Play;
	// EnterPlayを呼ぶのはエディタのボタン(PlayとRestart)だけなので、ここで立てれば足りる
	// (本番の開始はInitializeでmode_の初期値Playから始まり、ここを通らない)。
	// 一度立てたら、このシーンが終わるまで立てたままでよい
	startedFromEditor_ = true;
	// Playはゲームカメラ(イントロとレールカメラ)で映す。
	// デバッグカメラの視点はOFFにしても保たれるので、次にStopしたとき編集していた視点へ戻れる
	debugCamera_->SetActive(false);
	// 本番(タイトルやリトライから入ったとき)と同じく、イントロから始める
	introCamera_->Start();
}

void GamePlayScene::EnterEdit() {
	ResetGameState();
	mode_ = Mode::Edit;
	// Editは自由に見回すための編集用の視点、Playはゲームカメラ(イントロとレールカメラ)を使う。
	// DebugCamera::SetActiveは初回だけゲームカメラの視点から始め、2回目以降は前回の視点を保つので、
	// Stopすると前回編集していた視点に戻る。
	// プレイの出口(RequestResult)からUpdateの途中で呼ばれた場合、このフレームの描画カメラは
	// 決定済みなので、切り替わった視点が使われるのは次のフレームから
	debugCamera_->SetActive(true);
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

void GamePlayScene::DrawSectionsWindow() {
	ImGui::Begin("Sections");

	std::vector<StageData::SectionData>& sections = stage_->GetData().sections;
	const float railLength = stage_->GetRail().GetTotalLength();

	ImGui::Text("Distance: %.1f / %.1f m", railDistance_, railLength);
	if (sections.empty()) {
		ImGui::TextDisabled("No sections: the whole stage is one section.");
	}

	// i番目の区間が始まってよい範囲。前の区切りとも次の区切り(最後の区間なら終点)とも、
	// 最短の長さ以上あける。並び替えではなく「隣を追い越せない」ことで昇順を保つ:
	// ドラッグ中に並び替えると、つかんでいる行が別の区間へすり替わってしまうため
	auto startRange = [&](size_t index, float& outMin, float& outMax) {
		const float next = index + 1 < sections.size() ? sections[index + 1].startDistance : railLength;
		outMin = sections[index - 1].startDistance + kMinSectionLength;
		outMax = (std::max)(next - kMinSectionLength, outMin);
	};

	int removeIndex = -1;
	if (!sections.empty() &&
		ImGui::BeginTable("##sections", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("##operations", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < sections.size(); ++i) {
			StageData::SectionData& section = sections[i];
			const bool isLast = i + 1 == sections.size();
			const float end = isLast ? railLength : sections[i + 1].startDistance;
			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			// 今いる区間の行を色付けする(レールを進めながら、区切りの位置が合っているか確かめられる)
			const bool isCurrent = railDistance_ >= section.startDistance && (railDistance_ < end || isLast);
			if (isCurrent) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32({ 0.2f, 0.5f, 0.6f, 0.6f }));
			}

			// 名前。ImGuiのInputTextはstd::stringを直接扱えない(imgui_stdlibを入れていない)ので、固定長の作業領域を通す
			ImGui::TableSetColumnIndex(0);
			char nameBuffer[64];
			strncpy_s(nameBuffer, section.name.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer))) {
				section.name = nameBuffer;
			}

			// 開始距離。先頭は必ずレールの始点(0)なので編集させない
			ImGui::TableSetColumnIndex(1);
			float minStart = 0.0f;
			float maxStart = 0.0f;
			if (i == 0) {
				ImGui::TextDisabled("0.0 m (start)");
			} else {
				startRange(i, minStart, maxStart);
				ImGui::SetNextItemWidth(-FLT_MIN);
				// AlwaysClamp: Ctrl+クリックの直接入力でも範囲を越えさせない(越えると隣を追い越して昇順が崩れる)
				ImGui::DragFloat("##start", &section.startDistance, 0.5f, minStart, maxStart, "%.1f m",
					ImGuiSliderFlags_AlwaysClamp);
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.1f m", end);

			ImGui::TableSetColumnIndex(3);
			if (i > 0) {
				// 今のレール距離を開始にする(レールを進めて、区切りたい場所で押す)
				if (ImGui::SmallButton("Here")) {
					section.startDistance = std::clamp(railDistance_, minStart, maxStart);
				}
				ImGui::SameLine();
			}
			if (ImGui::SmallButton("Delete")) {
				removeIndex = static_cast<int>(i); // 一覧を回している最中なので、消すのはループの後
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (removeIndex >= 0) {
		sections.erase(sections.begin() + removeIndex);
		// 先頭を消したら、繰り上がった区間がレールの始点から始まるようにする(先頭は0の約束)
		if (!sections.empty()) {
			sections.front().startDistance = 0.0f;
		}
	}

	// 区間を足す。空なら始点(0)から、それ以外は今のレール距離で区切る。
	// 挿入位置は昇順が保たれる場所(既存の区切りとの前後関係で決まる)
	const float addStart = sections.empty() ? 0.0f : std::clamp(railDistance_, 0.0f, railLength);
	const auto insertAt = std::upper_bound(sections.begin(), sections.end(), addStart,
		[](float distance, const StageData::SectionData& section) { return distance < section.startDistance; });
	// 既存の区切りや終点に近すぎると長さほぼ0の区間ができるので、押せなくする
	bool tooClose = false;
	if (!sections.empty()) {
		tooClose = addStart - std::prev(insertAt)->startDistance < kMinSectionLength;
		const float next = insertAt != sections.end() ? insertAt->startDistance : railLength;
		tooClose = tooClose || next - addStart < kMinSectionLength;
	}
	ImGui::BeginDisabled(tooClose);
	if (ImGui::Button(sections.empty() ? "Add Section" : "Add Section Here")) {
		StageData::SectionData section;
		section.name = "Section " + std::to_string(sections.size() + 1);
		section.startDistance = addStart;
		sections.insert(insertAt, std::move(section));
	}
	ImGui::EndDisabled();
	if (tooClose) {
		ImGui::TextDisabled("Too close to a boundary. Move along the rail first.");
	}
	ImGui::TextDisabled("Save with Hierarchy > Stage File > Save.");

	ImGui::End();
}

void GamePlayScene::DrawLightingWindow() {
	ImGui::Begin("Lighting");

	StageData& data = stage_->GetData();
	// OFFにすると光を適用せず、エンジンの初期値(昼の見た目)になる。その状態でSaveするとlighting項目ごと書かれない
	ImGui::Checkbox("Stage Lighting", &data.hasLighting);

	// 値はStageDataを直接書き換える。光への反映はStage::Update(環境光・月明かり・点光源)と
	// UpdateHeadlight(ドローンのライト)が毎フレーム行うので、動かしながら見え方を確かめられる
	StageData::LightingData& lighting = data.lighting;
	ImGui::BeginDisabled(!data.hasLighting);
	ImGui::SeparatorText("Ambient");
	// どの光も届かない面に乗る明るさ。上げすぎると夜に見えず、下げすぎると敵や地形の輪郭が消える
	ImGui::ColorEdit3("Ambient", &lighting.ambientColor.x);

	ImGui::SeparatorText("Moon (Directional)");
	ImGui::ColorEdit3("Moon Color", &lighting.moonColor.x);
	ImGui::DragFloat("Moon Intensity", &lighting.moonIntensity, 0.01f, 0.0f, 5.0f, "%.2f");
	// 光が進む向き(長さはApplyLightsで1にそろえるので、向きだけ合わせればよい)
	ImGui::DragFloat3("Moon Direction", &lighting.moonDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");

	ImGui::SeparatorText("Drone Light (Spot)");
	ImGui::ColorEdit3("Light Color", &lighting.headlightColor.x);
	ImGui::DragFloat("Light Intensity", &lighting.headlightIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("Light Distance", &lighting.headlightDistance, 1.0f, 1.0f, 5000.0f, "%.0f");
	// 角度はラジアンで持ち、ImGuiのSliderAngleで度として表示・編集する(stage.jsonはラジアンの約束)
	ImGui::SliderAngle("Light Angle", &lighting.headlightAngle, 1.0f, 89.0f);
	ImGui::SliderAngle("Falloff Start", &lighting.headlightFalloffStart, 0.0f, 89.0f);
	ImGui::DragFloat("Light Decay", &lighting.headlightDecay, 0.01f, 0.01f, 10.0f, "%.2f");
	ImGui::EndDisabled();

	// 点光源の数。シェーダの配列の上限を超えた分は使われないので、その数を警告として出す
	ImGui::SeparatorText("Point Lights");
	const size_t lightCount = std::count_if(data.objects.begin(), data.objects.end(),
		[](const StageData::ObjectData& objectData) {
			return objectData.type == StageData::ObjectType::Light && !objectData.disabled;
		});
	ImGui::Text("Enabled: %zu / %u", lightCount, kMaxPointLights);
	if (stage_->GetPointLightOverflow() > 0) {
		ImGui::TextColored({ 1.0f, 0.4f, 0.3f, 1.0f }, "%u light(s) over the limit are ignored.",
			stage_->GetPointLightOverflow());
	}
	ImGui::TextDisabled("Add with Hierarchy > Stage Objects > Add Light.");
	ImGui::TextDisabled("Save with Hierarchy > Stage File > Save.");

	ImGui::End();
}
#endif

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;
