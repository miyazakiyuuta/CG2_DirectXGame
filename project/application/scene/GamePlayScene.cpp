#include "scene/GamePlayScene.h"

#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/DebugCamera.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "effect/CylinderManager.h"
#include "effect/ParticleConfig.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"
#include "io/Input.h"
#include "utility/Random.h"

#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "3d/Skybox.h"
#include "CameraController.h"
#include "Enemy/Manager/EnemyManager.h"
#include "Player.h"
#include "UI/ResultUI.h"
#include "Slug.h"
#include "StageEditor.h"
#include "StageSerializer.h"
#include "Tongue.h"
#include "UI/PauseMenu.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"
#include "effect/ParticleEmitter.h"
#include "math/Transform.h"
#include "scene/SceneManager.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "effect/ParticleManager.h"
#include "utility/Logger.h"
#include <numbers>
#include <sstream>

namespace {
EnemyType ClampEnemyTypeInt(int et) {
	if (et < 0) {
		et = 0;
	}
	const int kMaxEnemyType = static_cast<int>(EnemyType::PhaseGhost);
	if (et > kMaxEnemyType) {
		et = 0;
	}
	return static_cast<EnemyType>(et);
}
} // namespace

void GamePlayScene::InitializeEnemiesFromStage() {
	enemySpawns_.clear();
	enemyToSpawnIndex_.clear();
	enemiesInitializedFromStage_ = true;

	if (!stage_ || !enemyManager_) {
		return;
	}

	const auto spawns = stage_->GetEnemySpawnPoints();
	enemySpawns_.reserve(spawns.size());

	for (const auto& s : spawns) {
		EnemySpawnRuntime rt;
		rt.basePosition = s.position;
		rt.enemyType = static_cast<int>(ClampEnemyTypeInt(s.enemyType));
		rt.respawnIntervalSec = s.respawnInterval;
		if (rt.respawnIntervalSec < 0.0f) {
			rt.respawnIntervalSec = 0.0f;
		}
		rt.cooldownSec = 0.0f;
		rt.current = nullptr;
		enemySpawns_.push_back(rt);
	}

	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		SpawnEnemyForPoint(i);
	}
}

void GamePlayScene::SpawnEnemyForPoint(size_t idx) {
	if (!enemyManager_ || idx >= enemySpawns_.size()) {
		return;
	}

	auto& sp = enemySpawns_[idx];
	if (sp.current != nullptr) {
		return; // maxAlive=1
	}

	Vector3 pos = sp.basePosition;
	pos.y += enemySpawnYOffset_;

	BaseEnemy* created = enemyManager_->CreateEnemy(ClampEnemyTypeInt(sp.enemyType), pos);
	if (created) {
		sp.current = created;
		enemyToSpawnIndex_[created] = idx;
	}
}

void GamePlayScene::OnEnemyDead(BaseEnemy* e) {
	if (!e) {
		return;
	}

	auto it = enemyToSpawnIndex_.find(e);
	if (it == enemyToSpawnIndex_.end()) {
		return;
	}

	const size_t idx = it->second;
	enemyToSpawnIndex_.erase(it);
	if (idx >= enemySpawns_.size()) {
		return;
	}

	auto& sp = enemySpawns_[idx];
	if (sp.current == e) {
		sp.current = nullptr;
		sp.cooldownSec = sp.respawnIntervalSec;
	}
}

void GamePlayScene::UpdateEnemyRespawns(float deltaTime) {
	if (!enemyManager_ || enemySpawns_.empty()) {
		return;
	}

	// クールダウン更新と再スポーン
	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		auto& sp = enemySpawns_[i];
		if (sp.current != nullptr) {
			continue;
		}
		if (sp.cooldownSec > 0.0f) {
			sp.cooldownSec -= deltaTime;
			if (sp.cooldownSec > 0.0f) {
				continue;
			}
			sp.cooldownSec = 0.0f;
		}

		SpawnEnemyForPoint(i);
	}
}

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f});
	camera_->SetTranslate({0.0f, 7.5f, -20.0f});

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	// パーティクルグループの作成。第一引数はグループ名、第二引数はテクスチャファイルパス
	ParticleManager::GetInstance()->CreateParticleGroup("break", "resources/uvChecker.png");

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");
	TextureManager::GetInstance()->LoadTexture("resources/circle.png");
	TextureManager::GetInstance()->LoadTexture("resources/UI/KiwiMaruNumStrength.png");
	TextureManager::GetInstance()->LoadTexture("resources/UI/KiwiMaruColon.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("human", "sneakWalk.gltf");
	ModelManager::GetInstance()->LoadModel("Kanban1.obj");
	ModelManager::GetInstance()->LoadModel("Cube.obj");
	ModelManager::GetInstance()->LoadModel("tongue/tongue.obj");
	ModelManager::GetInstance()->LoadModel("human", "human_re.gltf");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");
	ModelManager::GetInstance()->LoadModel("Enemy/ClusterMinion", "ClusterMinion.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/GhostFace", "GhostFace.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/ProminenceSensor", "ProminenceSensor.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/SentinelHook", "SentinelHook.obj");

	// Load the single well model so it can be placed in the scene
	ModelManager::GetInstance()->LoadModel("well", "well.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("human_re.gltf");
	object3d_->SetCamera(camera_.get());
	object3d_->SetTranslate({0.0f, 0.0f, 5.0f});
	object3d_->SetRotate({0.0f, std::numbers::pi_v<float>, 0.0f});
	object3d_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
	object3d_->SetUseEnvironmentMap(true); // 環境マップ

	// Create the well object and place it at a fixed position only if model is loaded
	if (Object3dCommon::GetInstance() && camera_) {
		// Ensure model was actually loaded
		if (ModelManager::GetInstance()->FindModel("well.obj")) {
			wellObject_ = std::make_unique<Object3d>();
			wellObject_->Initialize(Object3dCommon::GetInstance());
			wellObject_->SetModel("well.obj");
			wellObject_->SetCamera(camera_.get());
			// Adjust this position as needed
			// Move the well slightly further from the camera and make it very small
			wellObject_->SetTranslate({0.0f, 0.0f, 0.0f});
			wellObject_->SetScale({60.0f, 500.0f, 60.0f});
			wellObject_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
			if (wellObject_) {
				Vector3 wellPos = wellObject_->GetTranslate();
				Vector3 wellScale = wellObject_->GetScale();

				wellCylinder_.center = wellPos;
				wellCylinder_.radius = 58.5f;
				wellCylinder_.halfHeight = 1000.0f;
			}
		} else {
			// Model not found; skip creating wellObject_
			wellObject_.reset();
		}
	} else {
		wellObject_.reset();
	}

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	// 実行時ステージを生成
	stage_ = std::make_unique<Stage>(Object3dCommon::GetInstance(), camera_.get());

	// ステージファイルの読み込みは GamePlayScene が担当
	auto loadedStage = StageSerializer::LoadFromFile("resources/stage.json");
	if (loadedStage) {
		stage_->SetStageData(*loadedStage);
	}

	// プレイヤー開始位置を Stage から取得
	Vector3 playerStart = {0.0f, 3.0f, 0.0f};
	if (auto spawn = stage_->GetPlayerSpawnPosition()) {
		playerStart = *spawn;
	}

	player_ = std::make_unique<Player>();
	player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Frog.gltf", playerStart);
	// give player a reference to the stage for abilities (camouflage lookup / sonar)
	player_->SetStage(stage_.get());

	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(camera_.get());
	cameraController_->SetTargetOffset({0.0f, 1.0f, 0.0f});
	cameraController_->SetDistance(25.0f);
	cameraController_->SetHeight(1.5f);
	cameraController_->SetYawSpeed(0.03f);
	cameraController_->SetPitchSpeed(0.02f);
	cameraController_->SetObstacleColliders(&stageBlockColliders_);
	cameraController_->SetObstacleCylinder(&wellCylinder_);
	cameraController_->SetKeepInsideCylinder(&wellCylinder_);

	player_->SetCameraController(cameraController_.get());
	player_->SetMovementLimitCylinder(&wellCylinder_);

	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize(SpriteCommon::GetInstance(), camera_.get(), cameraController_.get(), player_.get(), &stageBlockColliders_);

	// StageEditor は Stage を受け取って編集するだけ
	stageEditor_ = std::make_unique<StageEditor>(stage_.get(), Object3dCommon::GetInstance(), camera_.get());
	stageEditor_->Initialize("Cube.obj");

	// --- エネミーマネージャーの初期化 ---
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(Object3dCommon::GetInstance(), camera_.get());

	// XP orb pool 初期化
	xpOrbs_.resize(64); // 最大 64 個のオーブを同時管理

	// XP オーブのスポーン処理を登録
	enemyManager_->SetXPOrbSpawner([this](const Vector3& pos, AbilityId ability, int amount) {
		// 単純に amount 個のオーブを 1 値ずつスポーンするか、amount を分割して配る
		int remaining = amount;
		for (auto& orb : xpOrbs_) {
			if (remaining <= 0)
				break;
			if (!orb.IsActive()) {
				Vector3 spawnPos = pos;
				// ランダムな小さなオフセットを与えて見た目をばらす
				spawnPos.x += Random::GetFloat(-0.5f, 0.5f);
				spawnPos.y += Random::GetFloat(0.0f, 0.6f);
				spawnPos.z += Random::GetFloat(-0.5f, 0.5f);
				orb.SetAbility(ability);
				orb.Init(spawnPos, 1);
				// Do not expire automatically; remain until collected by player
				orb.SetFiniteLife(false);
				orb.SetGroundY(0.0f); // 後で毎フレーム上書きしてもよい
				--remaining;
			}
		}
	});

	// Stage から敵スポーン情報を取得して生成
	for (const auto& spawn : stage_->GetEnemySpawnPoints()) {
		const int typeValue = spawn.enemyType;

		// EnemyType と Stage 側の int の整合を軽くガード
		if (typeValue < static_cast<int>(EnemyType::Chasing) || typeValue > static_cast<int>(EnemyType::PhaseGhost)) {
			continue;
		}

		enemyManager_->CreateEnemy(static_cast<EnemyType>(typeValue), spawn.position);
	}

	//// 【エネミー出現位置】Y座標を5.0にしているので、空から降ってきて地面に着地します。
	// enemyManager_->CreateEnemy(EnemyType::Chasing, { 10.0f, 5.0f, 10.0f });        // 追尾（赤）
	// enemyManager_->CreateEnemy(EnemyType::Shooting, { -10.0f, 5.0f, 15.0f });	   // 射撃（青）
	// enemyManager_->CreateEnemy(EnemyType::Sentinel, { 0.0f, 5.0f, 0.0f });         // 逃走（オレンジ）
	// enemyManager_->CreateEnemy(EnemyType::ClusterSlime, { 5.0f, 8.0f, -5.0f });    // 群れ（紫）
	// enemyManager_->CreateEnemy(EnemyType::ProminenceSensor, {15.0f, 1.0f, 20.0f}); // センサー（固定砲台）
	// enemyManager_->CreateEnemy(EnemyType::PhaseGhost, {-15.0f, 8.0f, 0.0f});       // フェイズ・ゴースト（鳴き声で暴く敵）を生成

	// プレイヤーにエネミーマネージャーを渡して、プレイヤーからエネミーを参照できるようにする
	if (player_) {
		player_->SetEnemyManager(enemyManager_.get());
	}

	const float digitW = 32.0f;
	const float digitH = 32.0f;
	const float spacing = 2.0f;

	// : 画像も 32x32 前提
	const float colonW = 32.0f;
	const float colonH = 32.0f;

	// : と数字の左右の余白
	const float colonSideGap = 8.0f;

	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float timerY = 30.0f;

	// : を画面中央に置く
	const Vector2 colonPos = {centerX * 14 / 8 - colonW * 0.5f, timerY};

	// 左2桁の終端が「: の左余白」の位置に来るように、数字全体の開始位置を逆算
	const Vector2 timerBasePos = {colonPos.x - colonSideGap - (digitW * 2.0f + spacing), timerY};

	// SpriteNumberText の middleGap_ は「3桁目以降に追加する余白」
	// 通常 spacing ぶんは既にあるので、そのぶんを引いておく
	const float middleGap = colonW + colonSideGap * 2.0f - spacing;

	timerText_.Initialize(SpriteCommon::GetInstance(), "resources/UI/KiwiMaruNumStrength.png", 4);

	timerText_.SetPosition(timerBasePos);
	timerText_.SetDigitSize({digitW, digitH});
	timerText_.SetSpacing(spacing);
	timerText_.SetMiddleGap(middleGap);
	timerText_.SetColor({0.25f, 0.75f, 1.0f, 1.0f});
	timerText_.SetNumber(0, 4);

	// : 用スプライト
	timerColonSprite_ = std::make_unique<Sprite>();
	timerColonSprite_->Initialize(SpriteCommon::GetInstance(), "resources/UI/KiwiMaruColon.png");
	timerColonSprite_->SetAnchorPoint({0.0f, 0.0f});
	timerColonSprite_->SetSize({colonW, colonH});
	timerColonSprite_->SetPos(colonPos);
	timerColonSprite_->SetColor({0.25f, 0.75f, 1.0f, 1.0f});
	timerColonSprite_->Update();

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	Object3dCommon::GetInstance()->SetPointLight({
	    {1.0f, 1.0f, 1.0f, 1.0f}, // color
	    {0.0f, -0.1f, 0.0f}, // position
	    1.0f, // intensity
	    20000.0f, // radius
	    0.0f  // decay
	});

	// ポーズメニューの初期化
	pauseMenu_ = std::make_unique<PauseMenu>();
	// スプライト版の Initialize は SpriteCommon と CameraController を受け取る
	pauseMenu_->Initialize(SpriteCommon::GetInstance(), cameraController_.get());

	// リザルトUIの初期化
	resultUI_ = std::make_unique<ResultUI>();
	resultUI_->Initialize(SpriteCommon::GetInstance());

	// --- BGMの読み込みと再生 ---
	bgm_ = SoundManager::GetInstance()->LoadFile("resources/BGM/thirdStage.wav");
	bgmHandle_ = SoundManager::GetInstance()->PlayWave(bgm_, true, SoundManager::SoundCategory::BGM);

#ifndef USE_IMGUI
	// 起動時にプレイ状態のカーソル設定を適用
	while (ShowCursor(FALSE) >= 0) {
	}
	HWND hwnd = WinApp::GetInstance()->GetHwnd();
	RECT rect;
	GetClientRect(hwnd, &rect);
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.left));
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.right));
	ClipCursor(&rect);
#endif
}

void GamePlayScene::Finalize() {
	// シーン終了時、再生中のBGMボイスを確実に停止させてバッファ解放後のアクセスを防ぐ
	if (bgmHandle_ != SoundManager::InvalidHandle) {
		SoundManager::GetInstance()->StopWave(bgmHandle_);
		bgmHandle_ = SoundManager::InvalidHandle;
	}

	// BGMのアンロード
	SoundManager::GetInstance()->Unload("resources/BGM/thirdStage.wav");

	ParticleManager::GetInstance()->SetCamera(nullptr);

#ifndef USE_IMGUI

	// シーン終了時に必ず解除
	ShowCursor(TRUE);
	ClipCursor(nullptr);
#endif
}

void GamePlayScene::Update() {

	// Advance stage runtime (moving platforms, etc.) with fixed timestep
	// ポーズ中ではない場合のみブロック（ステージ）を動かすように修正
	if (stage_ && !pauseMenu_->IsPaused()) {
		stage_->Update(1.0f / 60.0f);

		auto deltas = stage_->ConsumePlatformDeltas();

		if (player_) {
			const Vector3 playerPos = player_->GetPosition();
			const Vector3 playerVel = player_->GetVelocity();
			const float playerHalfY = player_->GetColliderHalfHeight();
			const float playerBottom = playerPos.y - playerHalfY;

			// 上昇中は足場へ吸い付かないようにする
			const bool canSnapToPlatform = player_->IsOnGround() || playerVel.y <= 0.05f;

			bool foundRidePlatform = false;
			Vector3 bestApplyDelta = {0.0f, 0.0f, 0.0f};
			float bestScore = std::numeric_limits<float>::infinity();

			const float kRideCatchUpEps = 0.35f; // 上面より少し上にいても維持
			const float kRideSinkEps = 0.12f;    // 少しめり込んでいても維持
			const float kXZMargin = 0.45f;

			for (const auto& o : stage_->GetStageData().objects) {
				if (o.blockId != BlockID::MovingPlatform) {
					continue;
				}

				auto it = deltas.find(o.id);
				if (it == deltas.end()) {
					continue;
				}

				const Vector3 delta = it->second;
				const Vector3 prevPlatformPos = o.position - delta;

				const float halfX = o.scale.x;
				const float halfZ = o.scale.z;

				const float prevTopY = prevPlatformPos.y + o.scale.y;
				const float currTopY = o.position.y + o.scale.y;

				const bool insidePrevXZ = std::fabs(playerPos.x - prevPlatformPos.x) <= halfX + kXZMargin && std::fabs(playerPos.z - prevPlatformPos.z) <= halfZ + kXZMargin;

				const bool insideCurrXZ = std::fabs(playerPos.x - o.position.x) <= halfX + kXZMargin && std::fabs(playerPos.z - o.position.z) <= halfZ + kXZMargin;

				const bool wasStandingLastFrame = insidePrevXZ && playerBottom >= prevTopY - kRideSinkEps && playerBottom <= prevTopY + kRideCatchUpEps;

				const bool isLandingThisFrame = canSnapToPlatform && insideCurrXZ && playerBottom >= currTopY - kRideSinkEps && playerBottom <= currTopY + kRideCatchUpEps;

				if (!wasStandingLastFrame && !isLandingThisFrame) {
					continue;
				}

				Vector3 applyDelta = delta;

				// 今フレーム新しく乗った時だけ、Y を足場上面へ吸着
				if (!wasStandingLastFrame && isLandingThisFrame) {
					applyDelta.y = (currTopY + playerHalfY) - playerPos.y;
				}

				// 一番近い足場を採用
				float score = std::fabs(applyDelta.y);
				if (score < bestScore) {
					bestScore = score;
					bestApplyDelta = applyDelta;
					foundRidePlatform = true;
				}
			}

			if (foundRidePlatform) {
				player_->SetRidingPlatformDelta(bestApplyDelta);
			} else {
				player_->ClearRidingPlatformDelta();
			}
		}
	}

	// 実行時ステージから必要情報を取得
	stageBlockColliders_ = stage_->GetBlockOBBs();
	breakableBlockColliders_.clear();
	for (const auto& o : stage_->GetStageData().objects) {
		if (o.modelName != "Cube.obj") {
			continue;
		}
		if (o.blockId != BlockID::Breakable) {
			continue;
		}

		Transform t;
		t.translate = o.position;
		t.rotate = o.rotation;
		t.scale = o.scale;

		breakableBlockColliders_.push_back(CollisionUtility::MakeOBBFromTransform(t, {1.0f, 1.0f, 1.0f}));
	}
	player_->SetBreakableBlockColliders(&breakableBlockColliders_);
	waterBlockColliders_ = stage_->GetWaterBlockOBBs();
	player_->SetBlockColliders(&stageBlockColliders_);
	cameraController_->SetObstacleColliders(&stageBlockColliders_);
	if (reticle_) {
		reticle_->Update();

		if (player_) {
			if (reticle_->HasAimTargetPoint()) {
				player_->SetAimTargetPoint(reticle_->GetAimTargetPoint());
			} else {
				player_->ClearAimTargetPoint();
			}
		}
	}

	// エネミーマネージャーに当たり判定データを渡す
	if (enemyManager_) {
		enemyManager_->SetBlockColliders(&stageBlockColliders_);
		enemyManager_->SetKeepInsideCylinder(&wellCylinder_);
	}

	// --- リザルト演出中の場合は、演出のみ更新して早期リターン ---
	if (resultUI_ && resultUI_->IsActive()) {
		resultUI_->Update();
		if (resultUI_->IsTitleRequested()) {
			SceneManager::GetInstance()->ChangeScene("TITLE");
		}

		camera_->Update();
		camera_->TransferToGPU();
		return;
	}

	// 1. ポーズメニュー自体の更新（ESCキー判定など）
	pauseMenu_->Update();

#ifndef USE_IMGUI
	bool isPaused = pauseMenu_->IsPaused();

	// 状態が変わったときだけ切り替える
	if (isPaused != wasPaused_) {
		if (isPaused) {
			ShowCursor(TRUE);
			ClipCursor(nullptr);
		} else {
			ShowCursor(FALSE);
			HWND hwnd = WinApp::GetInstance()->GetHwnd();
			RECT rect;
			GetClientRect(hwnd, &rect);
			ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.left));
			ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.right));
			ClipCursor(&rect);
		}
		wasPaused_ = isPaused;
	}
#endif

	// 2. ポーズメニューからの要求（リスタート・終了）を処理
	// リスタート要求：SceneManager経由で新しいGamePlaySceneを生成する。
	// this->Initialize() を直接呼ぶと ParticleManager 等のシングルトンが
	// 二重初期化されて assert 落ちするため、シーンごと作り直す。
	if (pauseMenu_->IsRestartRequested()) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}
	if (pauseMenu_->IsTitleRequested()) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
		return;
	}

	// 3. 【核心】ポーズ中でない場合のみゲームの時間を動かす
	if (!pauseMenu_->IsPaused()) {

		gameTimer_.Update(1.0f / 60.0f);

		int totalSeconds = gameTimer_.GetDisplaySeconds();

		int minutes = totalSeconds / 60;
		int seconds = totalSeconds % 60;

		// 99:59 で止める
		if (minutes > 99) {
			minutes = 99;
			seconds = 59;
		}

		int displayValue = minutes * 100 + seconds;

		timerText_.SetNumber(displayValue, 4);
		timerText_.Update();

		// エディタモード中でなければ移動や衝突を更新
		if (!stageEditor_->IsEditMode()) {
			cameraController_->Update(player_->GetPosition());
			player_->Update();
			if (enemyManager_) {
				enemyManager_->Update(1.0f / 60.0f, player_.get());
			}

			if (player_) {
				player_->CheckEnemyContactDamage();
			}

			// XP オーブの更新とプレイヤーへの付与
			if (player_) {
				for (auto& orb : xpOrbs_) {
					if (!orb.IsActive())
						continue;
					// Update groundY from stage so orbs bounce and rest on floors/platforms
					if (stage_) {
						float gy = stage_->GetHeightAt(orb.GetPosition());
						orb.SetGroundY(gy);
					}
					int collected = orb.Update(1.0f / 60.0f, player_->GetPosition());
					if (collected > 0) {
						player_->EnqueueAbilityXP(orb.GetAbility(), static_cast<float>(collected));
					}
				}
			}

			// (描画インスタンス書き込みは ParticleManager::Update 後に行う)
		}

		// Warp detection: run before player update so teleport is immediate in gameplay mode
		if (player_) {
			const CollisionUtility::OBB playerObb = player_->GetPlayerOBB(player_->GetPosition());
			for (const auto& o : stage_->GetStageData().objects) {
				if (o.blockId != BlockID::Warp)
					continue;

				Transform t;
				t.translate = o.position;
				t.rotate = o.rotation;
				t.scale = o.scale;

				// Inflate warp OBB a bit to make triggering more forgiving
				const float warpInflation = 1.5f;
				CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t, {1.0f * o.scale.x * warpInflation, 1.0f * o.scale.y * warpInflation, 1.0f * o.scale.z * warpInflation});
				std::string msg = "Checking warp id:" + std::to_string(o.id) + " pos:" + std::to_string(o.position.x) + "," + std::to_string(o.position.y) + "," + std::to_string(o.position.z) + "\n";
				Logger::Log(msg);

				if (CollisionUtility::IntersectOBB_OBB(playerObb, obb)) {
					std::string hitmsg = "Player intersects warp id:" + std::to_string(o.id) + "\n";
					Logger::Log(hitmsg);

					if (warpCooldownCounter_ == 0 || lastWarpId_ != o.id) {
						// Request teleport to be applied inside the player's
						// own Update to avoid being overwritten by other systems.
						player_->SetPendingTeleport(o.warpTargetPosition);
						lastWarpId_ = o.id;
						// set a cooldown to avoid immediate re-trigger loops
						warpCooldownCounter_ = 90; // frames (~1.5 seconds at 60fps)
						Logger::Log(
						    std::string("Warped to ") + std::to_string(o.warpTargetPosition.x) + "," + std::to_string(o.warpTargetPosition.y) + "," + std::to_string(o.warpTargetPosition.z) + "\n");
					} else {
						Logger::Log(std::string("Warp ignored due to cooldown. id:") + std::to_string(o.id) + " counter:" + std::to_string(warpCooldownCounter_) + "\n");
					}
					break;
				}
			}

			if (warpCooldownCounter_ > 0)
				--warpCooldownCounter_;
			else
				lastWarpId_ = -1;
		}

		player_->UpdateTransparencyByCamera(camera_->GetTranslate());

		// 水ブロックに触れている間は徐々に回復
		bool isTouchingWater = false;
		if (player_) {
			const CollisionUtility::OBB playerObb = player_->GetPlayerOBB(player_->GetPosition());
			for (const auto& waterBox : waterBlockColliders_) {
				if (CollisionUtility::IntersectOBB_OBB(playerObb, waterBox)) {
					isTouchingWater = true;
					break;
				}
			}
		}

		if (isTouchingWater) {
			player_->AddWater(15.0f / 60.0f);
		}
	}

	// ----------------------------------------------------
	// トリガーチェック (Yキーでクリア、Uキーでゲームオーバー)
	// ----------------------------------------------------
	if (Input::GetInstance()->IsTriggerKey(DIK_Y)) {
		resultUI_->TriggerClear(gameTimer_.GetTimeSeconds());
		return;
	}

	if (Input::GetInstance()->IsTriggerKey(DIK_U)) {
		resultUI_->TriggerGameOver();
		return;
	}

	// (Warp detection handled earlier before player update.)

	camera_->Update();
	camera_->TransferToGPU();

	// ParticleManager 更新（インスタンス書き込みの前に最新のカメラ行列を使うため）
	ParticleManager::GetInstance()->Update(1.0f / 60.0f);

	// XP オーブ用インスタンス書き込み（Update 後、Draw 前）
	ParticleManager::GetInstance()->EnsureParticleGroup("xp_orb", "resources/circle.png");
	uint32_t maxInst = 0;
	auto* instPtr = ParticleManager::GetInstance()->GetInstancingDataWritePtr("xp_orb", maxInst);
	if (instPtr && maxInst > 0) {
		uint32_t numInst = 0;
		const Matrix4x4 viewProj = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
		Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();
		for (const auto& orb : xpOrbs_) {
			if (!orb.IsActive())
				continue;
			if (numInst >= maxInst)
				break;
			orb.FillInstanceData(instPtr[numInst], cameraMatrix, viewProj);
			++numInst;
		}
		ParticleManager::GetInstance()->SetExternalInstanceCount("xp_orb", numInst);
	}

	if (Input::GetInstance()->IsTriggerKey(DIK_0)) {
		object3d_->StopAnimation();
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_9)) {
		object3d_->StopAnimation(0.5f);
	}
	if (Input::GetInstance()->IsPushKey(DIK_1)) {
		object3d_->PlayAnimation("walk", true, 0.1f);
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_2)) {
		object3d_->PlayAnimation("sneakWalk", true, 0.1f);
	}

	object3d_->Update();
	if (wellObject_) {
		wellObject_->Update();
	}

	DebugRenderer::GetInstance()->AddGrid({0.0f, 0.0f, 0.0f}, 5.0f, 10, {0.0f, 0.0f, 0.0f, 1.0f});
}

void GamePlayScene::Draw() {
	// skybox_->Draw(*camera_);

	if (wellObject_) {
		wellObject_->Draw();
	}

	object3d_->Draw();
	// --- 不透明オブジェクトの描画 ---
	stage_->Draw();
	stageEditor_->Draw();
	player_->Draw();

	// エネミーの描画
	if (enemyManager_) {
		enemyManager_->Draw();
	}

	// パーティクル（XPオーブ等）を描画
	ParticleManager::GetInstance()->Draw();

	debugGrid_->Draw(*camera_);

	SpriteCommon::GetInstance()->CommonDrawSetting();
	if (reticle_) {
		reticle_->Draw();
	}
	player_->DrawUI();

	timerText_.Draw();

	if (timerColonSprite_) {
		timerColonSprite_->Draw();
	}
	// 3. ポーズUIを最後に重ねる（一番手前に表示）
	pauseMenu_->Draw();
	DebugRenderer::GetInstance()->RenderAll(*camera_);

	// リザルト演出の描画
	if (resultUI_->IsActive()) {
		resultUI_->Draw();
	}
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	Vector3 object3dPos = object3d_->GetTranslate();

	ImGui::Begin("GamePlayScene_Object");
	camera_->DrawImGui();
	if (ImGui::DragFloat3("object3d_->pos", &object3dPos.x, 0.01f)) {
		object3d_->SetTranslate(object3dPos);
	}
	ImGui::End();

	ImGui::Begin("a");
	// Well object debug / info
	if (wellObject_) {
		ImGui::Separator();
		ImGui::Text("Well Object");

		Vector3 wpos = wellObject_->GetTranslate();
		if (ImGui::DragFloat3("Well Position", &wpos.x, 0.1f)) {
			wellObject_->SetTranslate(wpos);
		}

		Vector3 wscale = wellObject_->GetScale();
		if (ImGui::DragFloat3("Well Scale", &wscale.x, 0.001f, 0.0001f, 100.0f)) {
			wellObject_->SetScale(wscale);
		}

		Vector3 wrot = wellObject_->GetRotate();
		ImGui::Text("Rotation: %.3f, %.3f, %.3f", wrot.x, wrot.y, wrot.z);
	}

	player_->DrawImGui();

	cameraController_->DrawImGui();
	if (enemyManager_) {
		enemyManager_->DrawImGui();
	}

	ImGui::End();

	stageEditor_->Update();

#endif
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;